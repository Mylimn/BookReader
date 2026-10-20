#include "ReaderView.h"

#include <QAbstractTextDocumentLayout>
#include <QEasingCurve>
#include <QPoint>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace reader::ui
{

ReaderView::ReaderView(QWidget *parent)
    : QTextBrowser(parent)
{
    setReadOnly(true);
    setFrameShape(QFrame::NoFrame);

    // 滚动位置或范围变化时刷新翻页按钮状态
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &ReaderView::updatePageState);
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, &ReaderView::updatePageState);
}

void ReaderView::showChapter(const reader::document::Chapter &chapter, int startOffset)
{
    m_chapter = chapter;
    m_hasChapter = true;

    document()->setHtml(m_renderer.toHtml(m_chapter));

    // 负偏移表示章末：取文档最后一个字符位置
    const int offset = (startOffset < 0) ? document()->characterCount() - 1 : startOffset;
    restoreToOffset(offset);
    updatePageState();
}

bool ReaderView::isAtTop() const
{
    const QScrollBar *bar = verticalScrollBar();
    return bar->value() <= bar->minimum();
}

bool ReaderView::isAtBottom() const
{
    const QScrollBar *bar = verticalScrollBar();
    return bar->value() >= bar->maximum();
}

void ReaderView::goToPreviousPage()
{
    animateScrollTo(verticalScrollBar()->value() - verticalScrollBar()->pageStep());
}

void ReaderView::goToNextPage()
{
    animateScrollTo(verticalScrollBar()->value() + verticalScrollBar()->pageStep());
}

void ReaderView::setFontSize(int px)
{
    if (px == m_renderer.fontSize()) {
        return;
    }

    // 先记录当前阅读位置，重排后回到同一处内容
    const int offset = currentCharOffset();
    m_renderer.setFontSize(px);

    if (m_hasChapter) {
        document()->setHtml(m_renderer.toHtml(m_chapter));
        restoreToOffset(offset);
        updatePageState();
    }
}

int ReaderView::fontSize() const
{
    return m_renderer.fontSize();
}

int ReaderView::currentCharOffset() const
{
    if (!m_hasChapter) {
        return 0;
    }
    // 视口左上角对应的字符位置即"当前页顶部"的内容位置
    return cursorForPosition(QPoint(0, 0)).position();
}

void ReaderView::updatePageState()
{
    const QScrollBar *bar = verticalScrollBar();
    emit pageStateChanged(bar->value() > bar->minimum(), bar->value() < bar->maximum());
}

void ReaderView::restoreToOffset(int offset)
{
    const int clamped = qBound(0, offset, document()->characterCount() - 1);

    QTextCursor cursor(document());
    cursor.setPosition(clamped);

    // 让该字符所在段落回到视口顶部：滚动条值取段落左上角的纵向坐标
    const QRectF blockRect = document()->documentLayout()->blockBoundingRect(cursor.block());
    verticalScrollBar()->setValue(static_cast<int>(blockRect.top()));
}

void ReaderView::animateScrollTo(int value)
{
    QScrollBar *bar = verticalScrollBar();
    const int target = qBound(bar->minimum(), value, bar->maximum());
    if (target == bar->value()) {
        return; // 已到边界，无需动画
    }

    if (m_pageAnimation == nullptr) {
        m_pageAnimation = new QPropertyAnimation(bar, "value", this);
        m_pageAnimation->setDuration(180);
        m_pageAnimation->setEasingCurve(QEasingCurve::OutCubic);
    }

    // 中途再次翻页：停止旧动画，从当前位置继续滑向新目标
    if (m_pageAnimation->state() == QAbstractAnimation::Running) {
        m_pageAnimation->stop();
    }
    m_pageAnimation->setStartValue(bar->value());
    m_pageAnimation->setEndValue(target);
    m_pageAnimation->start();
}

} // namespace reader::ui
