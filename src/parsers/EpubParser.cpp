#include "EpubParser.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QUrl>
#include <QVector>
#include <QXmlStreamReader>

#include <zip.h>

#include <memory>
#include <optional>

namespace reader::parsers
{

namespace
{

// ---- libzip 句柄的 RAII 包装：任何提前返回都会自动关闭压缩包 ----
struct ZipCloser
{
    void operator()(zip_t *archive) const
    {
        if (archive != nullptr) {
            zip_close(archive);
        }
    }
};

using ZipHandle = std::unique_ptr<zip_t, ZipCloser>;

// 把压缩包里的一个条目整份读进内存；条目不存在或解压不完整都返回 false
bool readEntry(zip_t *archive, const QString &entryName, QByteArray *content)
{
    const QByteArray name = entryName.toUtf8();

    zip_stat_t status;
    if (zip_stat(archive, name.constData(), ZIP_FL_ENC_UTF_8, &status) != 0) {
        return false;
    }

    zip_file_t *file = zip_fopen(archive, name.constData(), ZIP_FL_ENC_UTF_8);
    if (file == nullptr) {
        return false;
    }

    QByteArray data;
    data.resize(static_cast<qsizetype>(status.size));

    const qint64 expected = static_cast<qint64>(status.size);
    qint64 total = 0;
    while (total < expected) {
        const zip_int64_t bytes =
            zip_fread(file, data.data() + total, static_cast<zip_uint64_t>(expected - total));
        if (bytes <= 0) {
            break;
        }
        total += bytes;
    }
    zip_fclose(file);

    if (total != expected) {
        return false; // 宁可整体失败，也不给出残缺正文
    }

    *content = data;
    return true;
}

// 目录名：压缩包内路径用 '/' 分隔，根目录返回空串
QString parentDir(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return (slash >= 0) ? path.left(slash) : QString();
}

// 相对路径解析：去掉 #fragment 与 %xx 转义，再以 baseDir 为基准规范化 "." / ".."
QString resolvePath(const QString &baseDir, const QString &href)
{
    QString path = href;
    const qsizetype hash = path.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        path.truncate(hash);
    }
    path = QUrl::fromPercentEncoding(path.toUtf8());

    if (baseDir.isEmpty()) {
        return QDir::cleanPath(path);
    }
    return QDir::cleanPath(baseDir + QLatin1Char('/') + path);
}

// 从 container.xml 取第一个 rootfile 的 full-path：即 OPF 在压缩包内的位置
QString findOpfPath(zip_t *archive)
{
    QByteArray xml;
    if (!readEntry(archive, QStringLiteral("META-INF/container.xml"), &xml)) {
        return QString();
    }

    QXmlStreamReader reader(xml);
    while (reader.readNextStartElement()) {
        if (reader.name().toString() == QLatin1String("rootfile")) {
            return reader.attributes().value(QLatin1String("full-path")).toString();
        }
    }
    return QString();
}

// 清单项：OPF <manifest> 里的一条资源记录
struct ManifestItem
{
    QString href;
    QString mediaType;
};

// 阅读顺序项：OPF <spine> 里的一个引用
struct SpineItemRef
{
    QString idref;
    bool linear = true;
};

// 解析 OPF：metadata（书名 / 作者）、manifest（资源清单）、spine（阅读顺序）。
// 缺少清单或阅读顺序时返回 false，由上层提示解析失败（FR-01）。
bool parseOpf(const QByteArray &xml, reader::document::Book *book,
              QHash<QString, ManifestItem> *items, QVector<SpineItemRef> *spine)
{
    QXmlStreamReader reader(xml);
    QString section; // 当前所处的顶层容器：metadata / manifest / spine

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType token = reader.readNext();
        if (token != QXmlStreamReader::StartElement && token != QXmlStreamReader::EndElement) {
            continue;
        }

        const QString name = reader.name().toString();

        if (token == QXmlStreamReader::EndElement) {
            if (name == section) {
                section.clear();
            }
            continue;
        }

        if (name == QLatin1String("metadata") || name == QLatin1String("manifest")
            || name == QLatin1String("spine")) {
            section = name;
        } else if (section == QLatin1String("metadata") && name == QLatin1String("title")) {
            book->title = reader.readElementText().trimmed();
        } else if (section == QLatin1String("metadata") && name == QLatin1String("creator")) {
            book->author = reader.readElementText().trimmed();
        } else if (section == QLatin1String("manifest") && name == QLatin1String("item")) {
            const QXmlStreamAttributes attributes = reader.attributes();
            ManifestItem item;
            item.href = attributes.value(QLatin1String("href")).toString();
            item.mediaType = attributes.value(QLatin1String("media-type")).toString();
            const QString id = attributes.value(QLatin1String("id")).toString();
            if (!id.isEmpty() && !item.href.isEmpty()) {
                items->insert(id, item);
            }
        } else if (section == QLatin1String("spine") && name == QLatin1String("itemref")) {
            const QXmlStreamAttributes attributes = reader.attributes();
            SpineItemRef reference;
            reference.idref = attributes.value(QLatin1String("idref")).toString();
            reference.linear =
                attributes.value(QLatin1String("linear")).toString() != QLatin1String("no");
            if (!reference.idref.isEmpty()) {
                spine->append(reference);
            }
        }
    }

    return !reader.hasError() && !items->isEmpty() && !spine->isEmpty();
}

// 章节标题：优先正文里的第一个标题块（h1–h6），其次 XHTML 的 <title>，最后用文件名兜底
QString chapterTitle(const QTextDocument &document, const QString &href)
{
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() <= 0) {
            continue;
        }
        const QString text = block.text().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    }

    const QString documentTitle =
        document.metaInformation(QTextDocument::DocumentTitle).trimmed();
    if (!documentTitle.isEmpty()) {
        return documentTitle;
    }

    return QFileInfo(href).completeBaseName();
}

// 章节文档是 XHTML：交给 QTextDocument 解析（它对实体引用与不闭合标签都宽容），
// 再取纯文本。这样正文只保留段落结构，不会被书内自带的 CSS 覆盖主题配色。
reader::document::Chapter buildChapter(const QByteArray &xhtml, const QString &href)
{
    QTextDocument document;
    document.setHtml(QString::fromUtf8(xhtml));

    reader::document::Chapter chapter;
    chapter.content = document.toPlainText().trimmed();
    chapter.title = chapterTitle(document, href);
    return chapter;
}

} // namespace

std::optional<reader::document::Book> EpubParser::parse(const QString &filePath)
{
    int errorCode = 0;
    // libzip 在 Windows 上按 UTF-8 解释路径，中文目录也能正确打开
    const QByteArray archivePath = filePath.toUtf8();
    ZipHandle archive(zip_open(archivePath.constData(), ZIP_RDONLY, &errorCode));
    if (!archive) {
        return std::nullopt;
    }

    const QString opfPath = findOpfPath(archive.get());
    if (opfPath.isEmpty()) {
        return std::nullopt;
    }

    QByteArray opfXml;
    if (!readEntry(archive.get(), opfPath, &opfXml)) {
        return std::nullopt;
    }

    reader::document::Book book;
    QHash<QString, ManifestItem> items;
    QVector<SpineItemRef> spine;
    if (!parseOpf(opfXml, &book, &items, &spine)) {
        return std::nullopt;
    }

    // OPF 里的 href 以 OPF 自身所在目录为基准（常见为 OEBPS/、Text/ 等子目录）
    const QString baseDir = parentDir(opfPath);

    for (const SpineItemRef &reference : spine) {
        if (!reference.linear) {
            continue; // 阅读顺序之外的条目（封面、广告页等）不进入正文
        }

        const auto itemIt = items.constFind(reference.idref);
        if (itemIt == items.constEnd()) {
            continue; // OPF 自相矛盾：清单里找不到该项，跳过而不是整本失败
        }

        QByteArray xhtml;
        if (!readEntry(archive.get(), resolvePath(baseDir, itemIt->href), &xhtml)) {
            continue;
        }

        reader::document::Chapter chapter = buildChapter(xhtml, itemIt->href);
        if (chapter.content.isEmpty()) {
            continue; // 纯图片页等取不到文字，跳过避免整页空白
        }
        book.chapters.append(chapter);
    }

    if (book.chapters.isEmpty()) {
        return std::nullopt; // 没有任何可用正文：交由上层提示解析失败
    }
    if (book.title.isEmpty()) {
        book.title = QFileInfo(filePath).completeBaseName();
    }

    return book;
}

} // namespace reader::parsers
