#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QEasingCurve>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <optional>

#include "parsers/EpubParser.h"
#include "parsers/TxtParser.h"
#include "render/DocumentRenderer.h"
#include "ui/ReaderView.h"
#include "ui/TitleBar.h"

namespace reader::ui
{

namespace
{

// 阅读位置停止变化多久后落盘，避免滚动过程中频繁写文件
constexpr int kProgressSaveDelayMs = 800;

// 窗口四周为阴影预留的透明留白（最大化时收回为 0）
constexpr int kShadowMargin = 12;

// 边缘缩放：命中区宽度与窗口最小尺寸
constexpr int kResizeBorder = 6;
constexpr int kMinWindowWidth = 480;
constexpr int kMinWindowHeight = 360;

// 目录面板宽度
constexpr int kTocPanelWidth = 200;

// 传给 ReaderView::showChapter 的负偏移哨兵：表示定位到章末（跨章回翻用）
constexpr int kChapterEndOffset = -1;

ThemeMode themeFromString(const QString &value)
{
    return (value == QLatin1String("night")) ? ThemeMode::Night : ThemeMode::Day;
}

QString themeToString(ThemeMode mode)
{
    return (mode == ThemeMode::Night) ? QStringLiteral("night") : QStringLiteral("day");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 无边框 + 半透明背景：由内层 #windowFrame 承担背景、圆角与阴影
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    m_central = new QWidget(this);
    setCentralWidget(m_central);
    m_central->setMouseTracking(true);   // 悬停（未按键）也要收到 MouseMove 以切换光标
    m_central->installEventFilter(this); // 在透明留白区域做窗口边缘缩放
    m_outerLayout = new QVBoxLayout(m_central);
    m_outerLayout->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);

    m_frame = new QFrame(m_central);
    m_frame->setObjectName(QStringLiteral("windowFrame"));

    m_shadowEffect = new QGraphicsDropShadowEffect(m_frame);
    m_shadowEffect->setBlurRadius(28);
    m_shadowEffect->setOffset(0, 4);
    m_shadowEffect->setColor(QColor(0, 0, 0, 130));
    m_frame->setGraphicsEffect(m_shadowEffect);

    auto *frameLayout = new QVBoxLayout(m_frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    m_titleBar = new TitleBar(m_frame);
    m_readerView = new ReaderView(m_frame);

    // 阅读区 = 左侧目录面板 + 右侧正文；目录默认隐藏，由工具栏「目录」按钮切换（FR-03）
    m_contentArea = new QWidget(m_frame);
    auto *contentLayout = new QHBoxLayout(m_contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_tocPanel = new QWidget(m_contentArea);
    m_tocPanel->setObjectName(QStringLiteral("tocPanel"));
    m_tocPanel->setFixedWidth(kTocPanelWidth);

    auto *tocLayout = new QVBoxLayout(m_tocPanel);
    tocLayout->setContentsMargins(6, 8, 6, 8);
    m_tocList = new QListWidget(m_tocPanel);
    m_tocList->setObjectName(QStringLiteral("tocList"));
    m_tocList->setFrameShape(QFrame::NoFrame);
    tocLayout->addWidget(m_tocList);

    contentLayout->addWidget(m_tocPanel);
    contentLayout->addWidget(m_readerView, 1);
    m_tocPanel->setVisible(false);

    createActions(); // 创建 m_menuBar / m_toolBar（随后插入到布局中）

    frameLayout->addWidget(m_titleBar);
    frameLayout->addWidget(m_menuBar);
    frameLayout->addWidget(m_toolBar);
    frameLayout->addWidget(m_contentArea, 1);

    // 目录项单击 → 跳到对应章节
    connect(m_tocList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { showChapter(m_tocList->row(item), 0); });

    m_outerLayout->addWidget(m_frame);

    // 标题栏按钮 → 窗口操作
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, &MainWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);

    // 视图上报的翻页状态直接驱动按钮可用性；章末 / 章首还有相邻章节时也可翻（FR-03）
    connect(m_readerView, &ReaderView::pageStateChanged, this,
            [this](bool canGoPrevious, bool canGoNext) {
                m_previousPageAction->setEnabled(canGoPrevious || m_chapterIndex > 0);
                m_nextPageAction->setEnabled(
                    canGoNext
                    || static_cast<qsizetype>(m_chapterIndex) + 1 < m_currentBook.chapters.size());
            });

    // 阅读位置变化 → 节流后写入进度（FR-06）
    connect(m_readerView->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MainWindow::scheduleProgressSave);

    m_progressTimer = new QTimer(this);
    m_progressTimer->setSingleShot(true);
    m_progressTimer->setInterval(kProgressSaveDelayMs);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::saveProgressNow);

    // 恢复上次的字号与主题偏好（FR-04 / FR-05）
    const reader::storage::Settings settings = m_storage.loadSettings();
    m_readerView->setFontSize(settings.fontSize);
    applyTheme(themeFromString(settings.theme));

    updateWindowTitle(tr("Reader"));
    resize(900, 700);
}

void MainWindow::createActions()
{
    // ---- 菜单栏（内嵌在无边框容器里，而不是 QMainWindow 的原生菜单栏）----
    m_menuBar = new QMenuBar(m_frame);

    m_openAction = new QAction(tr("打开(&O)…"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);

    QMenu *fileMenu = m_menuBar->addMenu(tr("文件(&F)"));
    fileMenu->addAction(m_openAction);

    // ---- 视图菜单：主题切换（FR-05）----
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    m_dayAction = new QAction(tr("日间"), this);
    m_dayAction->setCheckable(true);
    themeGroup->addAction(m_dayAction);
    connect(m_dayAction, &QAction::triggered, this, [this] { applyTheme(ThemeMode::Day); });

    m_nightAction = new QAction(tr("夜间"), this);
    m_nightAction->setCheckable(true);
    themeGroup->addAction(m_nightAction);
    connect(m_nightAction, &QAction::triggered, this, [this] { applyTheme(ThemeMode::Night); });

    // ---- 视图菜单：字号调节（FR-04）----
    m_increaseFontAction = new QAction(tr("增大字号"), this);
    m_increaseFontAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+=")));
    connect(m_increaseFontAction, &QAction::triggered, this, &MainWindow::increaseFontSize);

    m_decreaseFontAction = new QAction(tr("减小字号"), this);
    m_decreaseFontAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    connect(m_decreaseFontAction, &QAction::triggered, this, &MainWindow::decreaseFontSize);

    QMenu *viewMenu = m_menuBar->addMenu(tr("视图(&V)"));
    viewMenu->addAction(m_dayAction);
    viewMenu->addAction(m_nightAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_increaseFontAction);
    viewMenu->addAction(m_decreaseFontAction);

    // ---- 阅读工具栏 ----
    m_tocAction = new QAction(tr("目录"), this);
    m_tocAction->setCheckable(true);
    connect(m_tocAction, &QAction::toggled, this, &MainWindow::setTocVisible);

    m_previousPageAction = new QAction(tr("上一页"), this);
    connect(m_previousPageAction, &QAction::triggered, this, &MainWindow::goToPreviousPage);

    m_nextPageAction = new QAction(tr("下一页"), this);
    connect(m_nextPageAction, &QAction::triggered, this, &MainWindow::goToNextPage);

    m_toolBar = new QToolBar(m_frame);
    m_toolBar->setMovable(false);
    m_toolBar->addAction(m_tocAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_previousPageAction);
    m_toolBar->addAction(m_nextPageAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_decreaseFontAction);
    m_toolBar->addAction(m_increaseFontAction);

    // 未打开书籍时不可翻页、也没有目录可看
    m_previousPageAction->setEnabled(false);
    m_nextPageAction->setEnabled(false);
    m_tocAction->setEnabled(false);
}

void MainWindow::applyTheme(ThemeMode mode)
{
    const ColorScheme scheme =
        (mode == ThemeMode::Night) ? Theme::nightScheme() : Theme::dayScheme();
    const QString styleSheet = Theme::buildStyleSheet(scheme);

    // 让菜单勾选与当前主题一致（启动时按存储的偏好恢复）
    m_dayAction->setChecked(mode == ThemeMode::Day);
    m_nightAction->setChecked(mode == ThemeMode::Night);

    // 主题偏好全局生效并持久化（FR-05）
    reader::storage::Settings settings = m_storage.loadSettings();
    settings.theme = themeToString(mode);
    m_storage.saveSettings(settings);

    if (!isVisible()) {
        // 启动阶段直接应用，不做动画
        qApp->setStyleSheet(styleSheet);
        return;
    }

    // 淡出 → 换肤 → 淡入，避免生硬瞬变
    if (m_themeAnimation != nullptr) {
        m_themeAnimation->stop();
        m_themeAnimation->deleteLater();
        m_themeAnimation = nullptr;
    }

    auto *fadeOut = new QPropertyAnimation(this, "windowOpacity");
    fadeOut->setDuration(120);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.55);

    auto *fadeIn = new QPropertyAnimation(this, "windowOpacity");
    fadeIn->setDuration(160);
    fadeIn->setStartValue(0.55);
    fadeIn->setEndValue(1.0);

    auto *group = new QSequentialAnimationGroup(this);
    group->addAnimation(fadeOut);
    group->addAnimation(fadeIn);

    // 在淡出结束时切换样式表，用户几乎看不到瞬间变化的边界
    connect(fadeOut, &QPropertyAnimation::finished, this,
            [styleSheet] { qApp->setStyleSheet(styleSheet); });

    m_themeAnimation = group;
    connect(group, &QSequentialAnimationGroup::finished, this, [this, group] {
        if (m_themeAnimation == group) {
            m_themeAnimation = nullptr;
        }
        group->deleteLater();
    });
    group->start();
}

void MainWindow::openFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("打开书籍"), QString(), tr("电子书 (*.epub *.txt)"));

    if (filePath.isEmpty()) {
        return; // 用户取消
    }

    loadBook(filePath);
}

void MainWindow::loadBook(const QString &filePath)
{
    // 换书前先把上一本的进度落盘
    saveProgressNow();

    const QFileInfo info(filePath);
    const QString canonicalPath = info.absoluteFilePath();
    const qint64 fileSize = info.size();

    // FR-01：按扩展名分发到对应解析器；两种格式解析后都产出同一个 Book 模型
    std::optional<reader::document::Book> parsed;
    const QString suffix = info.suffix().toLower();
    if (suffix == QLatin1String("epub")) {
        reader::parsers::EpubParser parser;
        parsed = parser.parse(canonicalPath);
    } else if (suffix == QLatin1String("txt")) {
        reader::parsers::TxtParser parser;
        parsed = parser.parse(canonicalPath);
    }

    if (!parsed.has_value() || parsed->chapters.isEmpty()) {
        // 解析失败给出提示并留在原界面，不崩溃（FR-01）
        QMessageBox::warning(this, tr("打开失败"),
                             tr("无法解析该文件，请确认它是有效的 EPUB 或 TXT 电子书。"));
        return;
    }

    m_currentBook = *parsed;

    // 读取该书的历史进度（FR-06）
    const reader::storage::BookProgress progress = m_storage.loadProgress(canonicalPath, fileSize);
    const int chapterIndex = (progress.chapterIndex >= 0
                              && progress.chapterIndex < m_currentBook.chapters.size())
                                 ? progress.chapterIndex
                                 : 0;

    m_currentBookPath = canonicalPath;
    m_currentBookSize = fileSize;

    populateToc();
    m_tocAction->setEnabled(true);
    setTocVisible(m_currentBook.chapters.size() > 1); // 多章节才有目录可看，默认展开

    showChapter(chapterIndex, progress.charOffset);
    updateWindowTitle(tr("%1 — Reader").arg(m_currentBook.title));

    // PRD 第 5 节：编码无法识别时已回退 UTF-8，正文可能有乱码，需要明确提示
    if (m_currentBook.encodingFallbackUsed) {
        QMessageBox::information(this, tr("编码提示"),
                                 tr("无法识别该文件的文本编码，已按 UTF-8 打开。"
                                    "若正文出现乱码，请把文件另存为 UTF-8 或 GBK 后再打开。"));
    }
}

void MainWindow::showChapter(int chapterIndex, int startOffset)
{
    if (chapterIndex < 0 || chapterIndex >= m_currentBook.chapters.size()) {
        return; // 越界：没有相邻章节可去，按钮已由 pageStateChanged 置灰（FR-03）
    }

    m_chapterIndex = chapterIndex;
    m_readerView->showChapter(m_currentBook.chapters.at(chapterIndex), startOffset);

    // 目录高亮跟随当前章节（setCurrentRow 不会触发 itemClicked，无需防重入）
    if (m_tocList->count() > chapterIndex) {
        m_tocList->setCurrentRow(chapterIndex);
    }

    saveProgressNow(); // 换章是离散动作，立即落盘
}

void MainWindow::populateToc()
{
    m_tocList->clear();
    for (const reader::document::Chapter &chapter : m_currentBook.chapters) {
        m_tocList->addItem(chapter.title);
    }
}

void MainWindow::setTocVisible(bool visible)
{
    m_tocPanel->setVisible(visible);
    if (m_tocAction->isChecked() != visible) {
        m_tocAction->setChecked(visible); // 与按钮状态保持一致（重入时状态已相同，不会递归）
    }
}

void MainWindow::goToPreviousPage()
{
    if (!m_readerView->isAtTop()) {
        m_readerView->goToPreviousPage();
        return;
    }
    showChapter(m_chapterIndex - 1, kChapterEndOffset); // 章首再上翻 → 落到上一章末尾
}

void MainWindow::goToNextPage()
{
    if (!m_readerView->isAtBottom()) {
        m_readerView->goToNextPage();
        return;
    }
    showChapter(m_chapterIndex + 1, 0); // 章末再下翻 → 进入下一章开头
}

void MainWindow::changeFontSize(int delta)
{
    const int current = m_readerView->fontSize();
    const int next = qBound(reader::render::kMinFontSize, current + delta,
                            reader::render::kMaxFontSize);
    if (next == current) {
        return; // 已到边界
    }

    m_readerView->setFontSize(next);

    // 字号全局生效并持久化（FR-04）
    reader::storage::Settings settings = m_storage.loadSettings();
    settings.fontSize = next;
    m_storage.saveSettings(settings);
}

void MainWindow::increaseFontSize()
{
    changeFontSize(reader::render::kFontSizeStep);
}

void MainWindow::decreaseFontSize()
{
    changeFontSize(-reader::render::kFontSizeStep);
}

void MainWindow::scheduleProgressSave()
{
    if (m_currentBookPath.isEmpty()) {
        return; // 未打开书籍
    }
    m_progressTimer->start(); // 重启计时：连续滚动只在停下后写一次
}

void MainWindow::saveProgressNow()
{
    if (m_currentBookPath.isEmpty()) {
        return;
    }

    reader::storage::BookProgress progress;
    progress.chapterIndex = m_chapterIndex;
    progress.charOffset = m_readerView->currentCharOffset();
    m_storage.saveProgress(m_currentBookPath, m_currentBookSize, progress);
}

void MainWindow::toggleMaximize()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::updateWindowTitle(const QString &title)
{
    // 原生标题不可见，需同步到自绘标题栏；同时保留系统任务栏标题
    m_titleBar->setTitle(title);
    setWindowTitle(title);
}

void MainWindow::updateFrameForWindowState()
{
    // 最大化时收回阴影留白并关闭阴影，否则窗口四周会露出透明缝隙
    const bool maximized = isMaximized();
    const int margin = maximized ? 0 : kShadowMargin;
    m_outerLayout->setContentsMargins(margin, margin, margin, margin);
    m_shadowEffect->setEnabled(!maximized);

    // 最大化时圆角也应收起：通过动态属性让样式表中的选择器生效
    m_frame->setProperty("maximized", maximized);
    m_frame->style()->unpolish(m_frame);
    m_frame->style()->polish(m_frame);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveProgressNow(); // 退出前确保进度落盘
    QMainWindow::closeEvent(event);
}

MainWindow::ResizeEdge MainWindow::resizeEdgeAt(const QPoint &windowPos) const
{
    const bool left = windowPos.x() <= kResizeBorder;
    const bool right = windowPos.x() >= width() - kResizeBorder;
    const bool top = windowPos.y() <= kResizeBorder;
    const bool bottom = windowPos.y() >= height() - kResizeBorder;

    if (top && left) {
        return ResizeEdge::TopLeft;
    }
    if (top && right) {
        return ResizeEdge::TopRight;
    }
    if (bottom && left) {
        return ResizeEdge::BottomLeft;
    }
    if (bottom && right) {
        return ResizeEdge::BottomRight;
    }
    if (left) {
        return ResizeEdge::Left;
    }
    if (right) {
        return ResizeEdge::Right;
    }
    if (top) {
        return ResizeEdge::Top;
    }
    if (bottom) {
        return ResizeEdge::Bottom;
    }
    return ResizeEdge::None;
}

void MainWindow::startResize(ResizeEdge edge, const QPoint &globalPos)
{
    m_resizeEdge = edge;
    m_resizeStartGeometry = geometry();
    m_resizeStartGlobal = globalPos;
}

void MainWindow::performResize(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_resizeStartGlobal;
    QRect geometry = m_resizeStartGeometry;

    const bool toLeft = (m_resizeEdge == ResizeEdge::Left
                         || m_resizeEdge == ResizeEdge::TopLeft
                         || m_resizeEdge == ResizeEdge::BottomLeft);
    const bool toRight = (m_resizeEdge == ResizeEdge::Right
                          || m_resizeEdge == ResizeEdge::TopRight
                          || m_resizeEdge == ResizeEdge::BottomRight);
    const bool toTop = (m_resizeEdge == ResizeEdge::Top
                        || m_resizeEdge == ResizeEdge::TopLeft
                        || m_resizeEdge == ResizeEdge::TopRight);
    const bool toBottom = (m_resizeEdge == ResizeEdge::Bottom
                           || m_resizeEdge == ResizeEdge::BottomLeft
                           || m_resizeEdge == ResizeEdge::BottomRight);

    if (toLeft) {
        geometry.setLeft(geometry.left() + delta.x());
    }
    if (toRight) {
        geometry.setRight(geometry.right() + delta.x());
    }
    if (toTop) {
        geometry.setTop(geometry.top() + delta.y());
    }
    if (toBottom) {
        geometry.setBottom(geometry.bottom() + delta.y());
    }

    // 约束最小尺寸；从左侧 / 上侧缩放时固定被拖的边、限制相对的边
    if (geometry.width() < kMinWindowWidth) {
        if (toLeft) {
            geometry.setLeft(geometry.right() - kMinWindowWidth);
        } else {
            geometry.setWidth(kMinWindowWidth);
        }
    }
    if (geometry.height() < kMinWindowHeight) {
        if (toTop) {
            geometry.setTop(geometry.bottom() - kMinWindowHeight);
        } else {
            geometry.setHeight(kMinWindowHeight);
        }
    }

    setGeometry(geometry);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_central || isMaximized()) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto cursorForEdge = [](ResizeEdge edge) -> Qt::CursorShape {
        switch (edge) {
        case ResizeEdge::Left:
        case ResizeEdge::Right:
            return Qt::SizeHorCursor;
        case ResizeEdge::Top:
        case ResizeEdge::Bottom:
            return Qt::SizeVerCursor;
        case ResizeEdge::TopLeft:
        case ResizeEdge::BottomRight:
            return Qt::SizeFDiagCursor;
        case ResizeEdge::TopRight:
        case ResizeEdge::BottomLeft:
            return Qt::SizeBDiagCursor;
        default:
            return Qt::ArrowCursor;
        }
    };

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }
        const QPoint windowPos = m_central->mapTo(this, mouseEvent->position().toPoint());
        const ResizeEdge edge = resizeEdgeAt(windowPos);
        if (edge == ResizeEdge::None) {
            break;
        }
        startResize(edge, mouseEvent->globalPosition().toPoint());
        return true;
    }
    case QEvent::MouseMove: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (m_resizeEdge != ResizeEdge::None && (mouseEvent->buttons() & Qt::LeftButton)) {
            performResize(mouseEvent->globalPosition().toPoint());
            return true;
        }
        const QPoint windowPos = m_central->mapTo(this, mouseEvent->position().toPoint());
        m_central->setCursor(cursorForEdge(resizeEdgeAt(windowPos)));
        break;
    }
    case QEvent::MouseButtonRelease:
        if (m_resizeEdge != ResizeEdge::None) {
            m_resizeEdge = ResizeEdge::None;
            return true;
        }
        break;
    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateFrameForWindowState();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (m_fadeInShown) {
        return; // 只在首次显示时淡入
    }
    m_fadeInShown = true;

    // 窗口出现淡入：从透明渐显到不透明
    setWindowOpacity(0.0);
    auto *animation = new QPropertyAnimation(this, "windowOpacity", this);
    animation->setDuration(240);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace reader::ui
