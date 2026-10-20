#pragma once

#include "document/IDocumentParser.h"

namespace reader::parsers
{

// TXT 解析器：调 EncodingDetector 识别 UTF-8 / UTF-16 / GBK（PRD 第 5 节），整本书切成单章。
class TxtParser : public reader::document::IDocumentParser
{
public:
    std::optional<reader::document::Book> parse(const QString &filePath) override;
};

} // namespace reader::parsers
