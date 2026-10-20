#pragma once

#include <QTextBrowser>

#include "document/Chapter.h"
#include "render/DocumentRenderer.h"

class QPropertyAnimation;

namespace reader::ui
{

// 阅读视图：展示正文，按视口高度整屏翻页，并上报当前是否可上/下翻页
class ReaderView : public QTextBrowser
{
    Q_OBJECT

public:
    explicit ReaderView(QWidget *parent = nullptr);

    // 载入章节内容，并定位到章内字符偏移 startOffset（用于恢复阅读进度）。
    // startOffset 为负表示定位到章末——跨章回翻时落在上一章的最后。
    void showChapter(const reader::document::Chapter &chapter, int startOffset = 0);

    void goToPreviousPage();
    void goToNextPage();

    // 视口是否已在本章顶部 / 底部：主窗口据此决定该翻页还是该换章（FR-03）
    bool isAtTop() const;
    bool isAtBottom() const;

    // 字号：重新排版后按字符偏移保持原有阅读位置（FR-04）
    void setFontSize(int px);
    int fontSize() const;

    // 当前视口顶部对应的章内字符偏移（章节开头为 0）（FR-06）
    int currentCharOffset() const;

signals:
    // 可翻页状态变化：用于让主窗口同步禁用 / 启用按钮（FR-03）
    void pageStateChanged(bool canGoPrevious, bool canGoNext);

private:
    void updatePageState();
    void restoreToOffset(int offset);
    void animateScrollTo(int value);

    reader::render::DocumentRenderer m_renderer;
    reader::document::Chapter m_chapter; // 记住当前章节，供字号变化时重排
    bool m_hasChapter = false;
    QPropertyAnimation *m_pageAnimation = nullptr; // 翻页滑动过渡
};

} // namespace reader::ui
