#pragma once

#include <QString>

#include "document/Chapter.h"

namespace reader::render
{

// 正文字号（px）：默认值、可调范围与步进，已定稿见 PRD 第 6 节
inline constexpr int kDefaultFontSize = 18;
inline constexpr int kMinFontSize = 12;
inline constexpr int kMaxFontSize = 32;
inline constexpr int kFontSizeStep = 2;

// 渲染器：把章节内容构建成 QTextDocument 可直接识别的 HTML。
// 样式（字号、行高）在此集中管理，视图层不关心排版细节。
class DocumentRenderer
{
public:
    explicit DocumentRenderer(int fontSize = kDefaultFontSize);

    void setFontSize(int px);
    int fontSize() const;

    // 纯文本 -> 带样式的 HTML 片段
    QString toHtml(const reader::document::Chapter &chapter) const;

private:
    int m_fontSize;
};

} // namespace reader::render
