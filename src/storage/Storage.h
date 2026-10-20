#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace reader::storage
{

// 全局偏好：对所有书生效
struct Settings
{
    int fontSize = 18;                          // 正文字号 px，默认 18（见 PRD 第 6 节）
    QString theme = QStringLiteral("day");      // 主题："day" / "night"
};

// 阅读进度：章节索引 + 章内字符偏移（见 TECH_DESIGN 3.3，不记页码）
struct BookProgress
{
    int chapterIndex = 0;
    int charOffset = 0;
};

// 存储：把偏好与阅读进度以 JSON 存于 QStandardPaths::AppDataLocation。
// 属于核心层，不依赖 UI / QWidget。
class Storage
{
public:
    Storage();

    Settings loadSettings() const;
    void saveSettings(const Settings &settings);

    // 书籍身份 = 规范化绝对路径 + 文件大小（见 TECH_DESIGN 3.4）
    BookProgress loadProgress(const QString &filePath, qint64 fileSize) const;
    void saveProgress(const QString &filePath, qint64 fileSize, const BookProgress &progress);

private:
    int indexOfBook(const QString &path, qint64 size) const;
    void flush();

    QString m_filePath;   // JSON 文件绝对路径
    QJsonObject m_root;   // 内存中的完整文档缓存
};

} // namespace reader::storage
