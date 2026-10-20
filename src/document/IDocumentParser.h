#pragma once

#include <optional>

#include "document/Book.h"

namespace reader::document
{

// 解析器统一接口：输入本地文件路径，成功产出 Book，失败返回空
class IDocumentParser
{
public:
    virtual ~IDocumentParser() = default;

    virtual std::optional<Book> parse(const QString &filePath) = 0;
};

} // namespace reader::document
