#include "Storage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace reader::storage
{

namespace
{

// 字段名集中在此，避免散落的字符串字面量
constexpr const char *kFileName = "reader.json";
constexpr const char *kSettingsKey = "settings";
constexpr const char *kBooksKey = "books";
constexpr const char *kPathKey = "path";
constexpr const char *kSizeKey = "size";
constexpr const char *kLastOpenedKey = "lastOpened";
constexpr const char *kProgressKey = "progress";
constexpr const char *kChapterIndexKey = "chapterIndex";
constexpr const char *kCharOffsetKey = "charOffset";
constexpr const char *kFontSizeKey = "fontSize";
constexpr const char *kThemeKey = "theme";

} // namespace

Storage::Storage()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir); // 首次运行目录可能不存在
    m_filePath = dir + QLatin1Char('/') + QLatin1String(kFileName);

    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            m_root = doc.object();
        }
    }
}

Settings Storage::loadSettings() const
{
    Settings settings;
    const QJsonObject obj = m_root.value(QLatin1String(kSettingsKey)).toObject();

    if (obj.contains(QLatin1String(kFontSizeKey))) {
        settings.fontSize = obj.value(QLatin1String(kFontSizeKey)).toInt(settings.fontSize);
    }
    if (obj.contains(QLatin1String(kThemeKey))) {
        settings.theme = obj.value(QLatin1String(kThemeKey)).toString(settings.theme);
    }
    return settings;
}

void Storage::saveSettings(const Settings &settings)
{
    QJsonObject obj;
    obj.insert(QLatin1String(kFontSizeKey), settings.fontSize);
    obj.insert(QLatin1String(kThemeKey), settings.theme);

    m_root.insert(QLatin1String(kSettingsKey), obj);
    flush();
}

int Storage::indexOfBook(const QString &path, qint64 size) const
{
    const QJsonArray books = m_root.value(QLatin1String(kBooksKey)).toArray();
    for (int i = 0; i < books.size(); ++i) {
        const QJsonObject book = books.at(i).toObject();
        const qint64 storedSize =
            static_cast<qint64>(book.value(QLatin1String(kSizeKey)).toDouble());
        if (book.value(QLatin1String(kPathKey)).toString() == path && storedSize == size) {
            return i;
        }
    }
    return -1;
}

BookProgress Storage::loadProgress(const QString &filePath, qint64 fileSize) const
{
    const QString path = QFileInfo(filePath).absoluteFilePath();
    const int index = indexOfBook(path, fileSize);
    if (index < 0) {
        return BookProgress{}; // 没有记录：回到章节开头
    }

    const QJsonObject progress =
        m_root.value(QLatin1String(kBooksKey)).toArray().at(index).toObject()
            .value(QLatin1String(kProgressKey)).toObject();

    BookProgress result;
    result.chapterIndex = progress.value(QLatin1String(kChapterIndexKey)).toInt(0);
    result.charOffset = progress.value(QLatin1String(kCharOffsetKey)).toInt(0);
    return result;
}

void Storage::saveProgress(const QString &filePath, qint64 fileSize, const BookProgress &progress)
{
    QJsonArray books = m_root.value(QLatin1String(kBooksKey)).toArray();
    const QString path = QFileInfo(filePath).absoluteFilePath();
    const int index = indexOfBook(path, fileSize);

    QJsonObject book;
    if (index >= 0) {
        book = books.at(index).toObject(); // 保留旧字段（如未来扩展的最近书信息）
        books.removeAt(index);
    }

    book.insert(QLatin1String(kPathKey), path);
    book.insert(QLatin1String(kSizeKey), static_cast<double>(fileSize));
    book.insert(QLatin1String(kLastOpenedKey),
                QDateTime::currentDateTime().toString(Qt::ISODate));

    QJsonObject progressObj;
    progressObj.insert(QLatin1String(kChapterIndexKey), progress.chapterIndex);
    progressObj.insert(QLatin1String(kCharOffsetKey), progress.charOffset);
    book.insert(QLatin1String(kProgressKey), progressObj);

    books.prepend(book); // 最近打开的书排在最前（即 TECH_DESIGN 5 的"最近书列表"）
    m_root.insert(QLatin1String(kBooksKey), books);
    flush();
}

void Storage::flush()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return; // 写失败不致命：本次进度丢失，下次位置变化会再尝试
    }
    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
}

} // namespace reader::storage
