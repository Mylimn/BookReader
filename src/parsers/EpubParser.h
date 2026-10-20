#pragma once

#include "document/IDocumentParser.h"

namespace reader::parsers
{

// EPUB 解析器：EPUB 本质是 ZIP 容器，先用 libzip 解压读取，再按规范逐层解析
//   META-INF/container.xml → OPF（metadata / manifest / spine）→ 各 spine 文档正文。
// XML 解析统一用 QXmlStreamReader（见 TECH_DESIGN 第 1 节）。
class EpubParser : public reader::document::IDocumentParser
{
public:
    std::optional<reader::document::Book> parse(const QString &filePath) override;
};

} // namespace reader::parsers
