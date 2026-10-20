#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
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

// 单书签（FR-08）：每本书最多一个位置标记，与进度同样是「章节索引 + 章内字符偏移」
struct Bookmark
{
    int chapterIndex = 0;
    int charOffset = 0;
};

// 最近打开的书（FR-07）：只承载事实（路径、大小、时间），展示名由 UI 层自行派生
struct RecentBook
{
    QString path;
    qint64 size = 0;
    QDateTime lastOpened;
};

// 存储：把偏好、阅读进度、书签与最近书列表以 JSON 存于 QStandardPaths::AppDataLocation。
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

    // 最近书列表（FR-07）：按打开时间倒序返回（books[] 中已按此顺序排列）
    QVector<RecentBook> loadRecentBooks() const;
    // 从最近列表移除该条目（FR-07）。只隐藏条目，仍保留阅读进度，
    // 以便再次打开时能续读（FR-06）——两者不冲突。
    void removeRecentBook(const QString &filePath, qint64 fileSize);

    // 单书签（FR-08）：返回该书的书签，没有则返回 false
    bool loadBookmark(const QString &filePath, qint64 fileSize, Bookmark &bookmark) const;
    void saveBookmark(const QString &filePath, qint64 fileSize, const Bookmark &bookmark);
    void clearBookmark(const QString &filePath, qint64 fileSize);

private:
    int indexOfBook(const QString &path, qint64 size) const;
    // 从 books 数组中摘出该书条目（不存在则返回空对象），交由调用方补字段后自行 prepend
    QJsonObject takeBook(QJsonArray &books, const QString &path, qint64 size) const;
    void flush();

    QString m_filePath;   // JSON 文件绝对路径
    QJsonObject m_root;   // 内存中的完整文档缓存
};

} // namespace reader::storage
