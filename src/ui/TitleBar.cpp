#include "TitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>

namespace reader::ui
{

namespace
{

// 创建一个标题栏按钮：统一样式由 QSS 的 #titleBarButton 负责
QToolButton *makeButton(QWidget *parent, const QString &text, const QString &toolTip)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("titleBarButton"));
    button->setText(text);
    button->setToolTip(toolTip);
    button->setFocusPolicy(Qt::NoFocus); // 避免按钮抢走键盘焦点
    // QToolButton 默认是"仅图标"样式，只设文字时必须显式切到"仅文字"
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setAutoRaise(true);
    return button;
}

} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("titleBar"));
    setFixedHeight(44);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("titleBarLabel"));

    m_minimizeButton = makeButton(this, QStringLiteral("—"), tr("最小化"));
    connect(m_minimizeButton, &QToolButton::clicked, this, &TitleBar::minimizeRequested);

    m_maximizeButton = makeButton(this, QStringLiteral("□"), tr("最大化 / 还原"));
    connect(m_maximizeButton, &QToolButton::clicked, this, &TitleBar::maximizeRestoreRequested);

    m_closeButton = makeButton(this, QStringLiteral("✕"), tr("关闭"));
    m_closeButton->setObjectName(QStringLiteral("titleBarCloseButton")); // 关闭键单独配色
    connect(m_closeButton, &QToolButton::clicked, this, &TitleBar::closeRequested);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 8, 0);
    layout->setSpacing(4);
    layout->addWidget(m_titleLabel);
    layout->addStretch();
    layout->addWidget(m_minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(m_closeButton);
}

void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - window()->pos();
    event->accept();
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || !(event->buttons() & Qt::LeftButton) || window()->isMaximized()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // 全局鼠标坐标减去固定偏移 = 窗口新位置，拖动时鼠标相对标题栏的位置不变
    window()->move(event->globalPosition().toPoint() - m_dragOffset);
    event->accept();
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeRestoreRequested();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace reader::ui
