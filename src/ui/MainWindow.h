#pragma once

#include <QMainWindow>

#include "document/Book.h"
#include "storage/Storage.h"
#include "ui/Theme.h"

class QAction;
class QCloseEvent;
class QEvent;
class QFrame;
class QGraphicsDropShadowEffect;
class QListWidget;
class QMenuBar;
class QSequentialAnimationGroup;
class QShowEvent;
class QStackedWidget;
class QTimer;
class QToolBar;
class QVBoxLayout;

namespace reader::ui
{

class ReaderView;
class StartPage;
class TitleBar;

// 主窗口：无边框窗口（内层圆角容器 + 自绘标题栏）+ 菜单 + 工具栏 + 阅读视图
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openFile();
    void goHome();
    void goToPreviousPage();
    void goToNextPage();
    void increaseFontSize();
    void decreaseFontSize();
    void toggleBookmark();
    void markBookmark();
    void jumpToBookmark();
    void clearBookmark();
    void scheduleProgressSave();
    void saveProgressNow();
    void toggleMaximize();

private:
    // 窗口边缘的 8 个缩放方向
    enum class ResizeEdge
    {
        None,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    void createActions();
    void applyTheme(ThemeMode mode);
    void changeFontSize(int delta);
    void loadBook(const QString &filePath);

    // ---- 起始页（FR-07）----
    void showStartPage();          // 切到最近书起始页
    void showReaderPage();         // 切回阅读视图
    void refreshRecentBooks();     // 用存储中的最近书列表刷新起始页
    void removeRecentBook(const QString &filePath, qint64 fileSize);

    // ---- 单书签（FR-08）----
    void refreshBookmarkState();   // 打开书籍时读取该书书签
    void updateBookmarkUi();       // 按"是否已标记"同步按钮与菜单项

    // 统一切换章节的入口：越界不动作，切换后同步目录高亮并落盘进度
    void showChapter(int chapterIndex, int startOffset);
    void populateToc();
    void setTocVisible(bool visible);

    void updateWindowTitle(const QString &title);
    void updateFrameForWindowState();

    // ---- 无边框窗口的边缘缩放 ----
    ResizeEdge resizeEdgeAt(const QPoint &windowPos) const;
    void startResize(ResizeEdge edge, const QPoint &globalPos);
    void performResize(const QPoint &globalPos);

    // ---- 无边框窗口结构 ----
    QWidget *m_central = nullptr;
    QVBoxLayout *m_outerLayout = nullptr;
    QFrame *m_frame = nullptr;
    QGraphicsDropShadowEffect *m_shadowEffect = nullptr;
    TitleBar *m_titleBar = nullptr;
    QMenuBar *m_menuBar = nullptr;
    QToolBar *m_toolBar = nullptr;

    ReaderView *m_readerView = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_homeAction = nullptr;          // 返回最近书起始页（FR-07）
    QAction *m_previousPageAction = nullptr;
    QAction *m_nextPageAction = nullptr;
    QAction *m_tocAction = nullptr;
    QAction *m_increaseFontAction = nullptr;
    QAction *m_decreaseFontAction = nullptr;
    QAction *m_dayAction = nullptr;
    QAction *m_nightAction = nullptr;

    // ---- 单书签（FR-08）----
    QAction *m_bookmarkAction = nullptr;      // 工具栏上的情境按钮：未标记=标记，已标记=跳转
    QAction *m_markBookmarkAction = nullptr;
    QAction *m_jumpBookmarkAction = nullptr;
    QAction *m_clearBookmarkAction = nullptr;

    // ---- 内容区：起始页 / 阅读视图两页互切（FR-07）----
    QStackedWidget *m_contentStack = nullptr;
    QWidget *m_readerArea = nullptr;          // 阅读页：左侧目录面板 + 右侧正文
    StartPage *m_startPage = nullptr;
    QWidget *m_tocPanel = nullptr;
    QListWidget *m_tocList = nullptr;

    reader::storage::Storage m_storage;
    QTimer *m_progressTimer = nullptr;

    // 边缘缩放的进行态
    ResizeEdge m_resizeEdge = ResizeEdge::None;
    QRect m_resizeStartGeometry;
    QPoint m_resizeStartGlobal;

    // 过渡动画状态
    bool m_fadeInShown = false;                        // 窗口出现淡入只做一次
    QSequentialAnimationGroup *m_themeAnimation = nullptr; // 主题切换的淡出→换肤→淡入

    // 当前打开的书籍身份与进度上下文（FR-06）
    reader::document::Book m_currentBook; // 供目录与跨章翻页使用（FR-03）
    QString m_currentBookPath;   // 规范化绝对路径；为空表示当前停在起始页
    qint64 m_currentBookSize = 0;
    int m_chapterIndex = 0;

    // 当前书的单书签状态（FR-08）
    bool m_hasBookmark = false;
    reader::storage::Bookmark m_bookmark;
};

} // namespace reader::ui
