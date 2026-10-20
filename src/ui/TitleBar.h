#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QLabel;
class QToolButton;

namespace reader::ui
{

// 自绘标题栏：书名 + 最小化 / 最大化(还原) / 关闭 三个按钮。
// 拖动空白处移动窗口，双击最大化 / 还原（无边框窗口自带系统标题栏已被移除）。
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);

signals:
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QLabel *m_titleLabel = nullptr;
    QToolButton *m_minimizeButton = nullptr;
    QToolButton *m_maximizeButton = nullptr;
    QToolButton *m_closeButton = nullptr;

    bool m_dragging = false;
    QPoint m_dragOffset; // 按下时鼠标相对窗口左上角的偏移，移动时保持不变
};

} // namespace reader::ui
