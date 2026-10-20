#pragma once

#include <QString>

namespace reader::document
{

// 章节：一本书中的一段内容。所有格式解析后都产出该结构（见 TECH_DESIGN 3.1）
struct Chapter
{
    QString title;   // 章节标题（TXT 单章时取书名）
    QString content; // 章节原始内容（纯文本，由渲染器负责转成可显示的 HTML）
};

} // namespace reader::document
