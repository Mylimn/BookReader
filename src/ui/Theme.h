#pragma once

#include <QColor>
#include <QString>

namespace reader::ui
{

// 主题模式：日间 / 夜间
enum class ThemeMode
{
    Day,
    Night
};

// 一套完整配色：主题涉及的全部颜色集中在此，样式表只引用 token，不含具体色值
struct ColorScheme
{
    QColor windowBg;          // 窗口底色（菜单栏、工具栏）
    QColor surface;           // 浮层底色（菜单面板）
    QColor hover;             // 悬停反馈
    QColor pressed;           // 按下反馈
    QColor border;            // 分隔线与描边
    QColor text;              // 常规文字
    QColor textMuted;         // 次要文字（禁用态）
    QColor pageBg;            // 阅读区底色
    QColor pageText;          // 阅读区正文
    QColor accent;            // 强调色（选中）
    QColor scrollHandle;      // 滚动条滑块
    QColor scrollHandleHover; // 滚动条滑块（悬停）
    QColor closeHover;        // 标题栏关闭按钮（悬停 / 按下）
};

// 主题：提供预设配色，并把配色套进样式表模板
class Theme
{
public:
    static ColorScheme dayScheme();
    static ColorScheme nightScheme();

    // 读取 :/theme.qss 模板，将 @token@ 替换为配色色值，返回可直接应用的样式表
    static QString buildStyleSheet(const ColorScheme &scheme);
};

} // namespace reader::ui
