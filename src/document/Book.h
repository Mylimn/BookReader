#pragma once

#include <QString>
#include <QVector>

#include "document/Chapter.h"

namespace reader::document
{

// 书籍：格式无关的统一文档模型，上层渲染 / 目录 / 翻页只面向它
struct Book
{
    QString title;             // 书名
    QString author;            // 作者（TXT 无法提供该信息时留空）
    QVector<Chapter> chapters; // 章节列表

    // 解析中的非致命问题：TXT 编码无法识别、已回退 UTF-8，正文可能有乱码（PRD 第 5 节）。
    // 这里只承载事实，面向用户的提示文案由 UI 层给出，核心层不产出界面文案。
    bool encodingFallbackUsed = false;
};

} // namespace reader::document
