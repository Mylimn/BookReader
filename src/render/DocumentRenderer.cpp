#include "DocumentRenderer.h"

#include <QStringList>

namespace reader::render
{

DocumentRenderer::DocumentRenderer(int fontSize)
    : m_fontSize(fontSize)
{
}

void DocumentRenderer::setFontSize(int px)
{
    m_fontSize = px;
}

int DocumentRenderer::fontSize() const
{
    return m_fontSize;
}

QString DocumentRenderer::toHtml(const reader::document::Chapter &chapter) const
{
    // 统一换行：Windows 文本是 CRLF，先规范化避免行尾残留 '\r'
    const QString normalized = QString(chapter.content).replace(QStringLiteral("\r\n"),
                                                                QStringLiteral("\n"));

    // 纯文本按换行拆段：非空行包成 <p>，空行作为一个空段落保持段间距
    const QStringList lines = normalized.split(QLatin1Char('\n'));

    QString body;
    body.reserve(normalized.size() + lines.size() * 8);
    for (const QString &line : lines) {
        const QString text = line.toHtmlEscaped();
        if (text.isEmpty()) {
            body += QStringLiteral("<p>&nbsp;</p>");
        } else {
            body += QStringLiteral("<p>%1</p>").arg(text);
        }
    }

    return QStringLiteral("<html><body style=\"font-size:%1px; line-height:1.6;\">%2</body></html>")
        .arg(m_fontSize)
        .arg(body);
}

} // namespace reader::render
