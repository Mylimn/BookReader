#include "Theme.h"

#include <QFile>

namespace reader::ui
{

namespace
{

// 一个 token 与其对应色值的替换项
struct TokenColor
{
    const char *token;
    QColor ColorScheme::*member;
};

// token 与 ColorScheme 成员的映射表：新增颜色只需在此补一行
const TokenColor kTokens[] = {
    {"@window-bg@", &ColorScheme::windowBg},
    {"@surface@", &ColorScheme::surface},
    {"@hover@", &ColorScheme::hover},
    {"@pressed@", &ColorScheme::pressed},
    {"@border@", &ColorScheme::border},
    {"@text-muted@", &ColorScheme::textMuted},
    {"@text@", &ColorScheme::text},
    {"@page-bg@", &ColorScheme::pageBg},
    {"@page-text@", &ColorScheme::pageText},
    {"@accent@", &ColorScheme::accent},
    {"@scroll-handle-hover@", &ColorScheme::scrollHandleHover},
    {"@scroll-handle@", &ColorScheme::scrollHandle},
    {"@close-hover@", &ColorScheme::closeHover},
};

} // namespace

ColorScheme Theme::dayScheme()
{
    ColorScheme scheme;
    scheme.windowBg = QColor(QStringLiteral("#F5F2EC"));          // 暖白
    scheme.surface = QColor(QStringLiteral("#FBF9F5"));
    scheme.hover = QColor(QStringLiteral("#E9E3D8"));
    scheme.pressed = QColor(QStringLiteral("#DED7C9"));
    scheme.border = QColor(QStringLiteral("#E3DED4"));
    scheme.text = QColor(QStringLiteral("#2B2A27"));
    scheme.textMuted = QColor(QStringLiteral("#A39C90"));
    scheme.pageBg = QColor(QStringLiteral("#FBF9F5"));
    scheme.pageText = QColor(QStringLiteral("#2B2A27"));
    scheme.accent = QColor(QStringLiteral("#B08968"));
    scheme.scrollHandle = QColor(QStringLiteral("#D5CEC1"));
    scheme.scrollHandleHover = QColor(QStringLiteral("#B9B0A0"));
    scheme.closeHover = QColor(QStringLiteral("#C42B1C"));
    return scheme;
}

ColorScheme Theme::nightScheme()
{
    ColorScheme scheme;
    scheme.windowBg = QColor(QStringLiteral("#1E1F22"));          // 深灰，非纯黑
    scheme.surface = QColor(QStringLiteral("#26282C"));
    scheme.hover = QColor(QStringLiteral("#31343A"));
    scheme.pressed = QColor(QStringLiteral("#3A3E45"));
    scheme.border = QColor(QStringLiteral("#33363B"));
    scheme.text = QColor(QStringLiteral("#D6D3CE"));
    scheme.textMuted = QColor(QStringLiteral("#75726B"));
    scheme.pageBg = QColor(QStringLiteral("#1E1F22"));
    scheme.pageText = QColor(QStringLiteral("#CFCCC6"));
    scheme.accent = QColor(QStringLiteral("#C39A6B"));
    scheme.scrollHandle = QColor(QStringLiteral("#3C3F45"));
    scheme.scrollHandleHover = QColor(QStringLiteral("#4E525A"));
    scheme.closeHover = QColor(QStringLiteral("#B03A2E"));
    return scheme;
}

QString Theme::buildStyleSheet(const ColorScheme &scheme)
{
    QFile file(QStringLiteral(":/theme.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString(); // 资源缺失：返回空样式表，界面回退到系统默认外观
    }

    QString styleSheet = QString::fromUtf8(file.readAll());
    file.close();

    for (const TokenColor &item : kTokens) {
        styleSheet.replace(QLatin1String(item.token), (scheme.*(item.member)).name());
    }

    return styleSheet;
}

} // namespace reader::ui
