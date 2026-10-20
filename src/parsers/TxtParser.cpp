#include "TxtParser.h"

#include <QFile>
#include <QFileInfo>

#include "utils/EncodingDetector.h"

namespace reader::parsers
{

std::optional<reader::document::Book> TxtParser::parse(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt; // 打不开：由调用方给出用户提示（FR-01）
    }

    const QByteArray raw = file.readAll();
    file.close();

    // 编码识别（PRD 第 5 节）：UTF-8 / UTF-16 / GBK，认不出来则回退 UTF-8 并标记，由 UI 提示
    const reader::utils::EncodingDetector::Result decoded =
        reader::utils::EncodingDetector::decode(raw);

    reader::document::Chapter chapter;
    chapter.title = QFileInfo(filePath).completeBaseName();
    chapter.content = decoded.text;

    reader::document::Book book;
    book.title = chapter.title;
    book.chapters.append(chapter);
    book.encodingFallbackUsed = !decoded.reliable;

    return book;
}

} // namespace reader::parsers
