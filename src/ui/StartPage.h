#pragma once

#include <QVector>
#include <QWidget>

#include "storage/Storage.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace reader::ui
{

// 起始页（FR-07）：应用启动或返回首页时展示最近打开过的书籍。
// 只负责展示与收集用户意图，真正的存储读写由 MainWindow 完成——
// 因此这里通过信号上报，不直接持有 Storage。
class StartPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartPage(QWidget *parent = nullptr);

    // 刷新列表内容（按最近打开时间倒序，已由 Storage 排好）
    void setRecentBooks(const QVector<reader::storage::RecentBook> &books);

signals:
    void openRequested(const QString &filePath);            // 点击条目：再次打开该书
    void removeRequested(const QString &filePath, qint64 fileSize); // 右键：从最近列表移除
    void browseRequested();                                 // 空态按钮 / 条目为空时打开文件对话框

private:
    QListWidget *m_list = nullptr;
    QLabel *m_emptyHint = nullptr;
    QPushButton *m_browseButton = nullptr;
};

} // namespace reader::ui
