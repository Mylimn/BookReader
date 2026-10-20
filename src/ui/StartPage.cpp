#include "StartPage.h"

#include <QAction>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace reader::ui
{

namespace
{

// 条目上挂的自定义数据：路径与文件大小（移除时用于定位存储中的条目）
constexpr int kPathRole = Qt::UserRole;
constexpr int kSizeRole = Qt::UserRole + 1;

} // namespace

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("startPage"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 36, 48, 36);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("最近打开"), this);
    title->setObjectName(QStringLiteral("startPageTitle"));

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("recentList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    m_emptyHint = new QLabel(tr("还没有读过任何书，点下面按钮打开一本。"), this);
    m_emptyHint->setObjectName(QStringLiteral("startPageHint"));
    m_emptyHint->setAlignment(Qt::AlignCenter);

    m_browseButton = new QPushButton(tr("打开书籍…"), this);
    m_browseButton->setObjectName(QStringLiteral("startPageBrowse"));
    m_browseButton->setCursor(Qt::PointingHandCursor);

    layout->addWidget(title);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_emptyHint, 1);
    layout->addWidget(m_browseButton, 0, Qt::AlignHCenter);

    // 单击条目即再次打开（FR-07："一键再次打开"）
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit openRequested(item->data(kPathRole).toString());
    });

    // 右键条目：打开 / 从最近列表移除
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_list->itemAt(pos);
        if (item == nullptr) {
            return;
        }

        QMenu menu(this);
        QAction *openAction = menu.addAction(tr("打开"));
        menu.addSeparator();
        QAction *removeAction = menu.addAction(tr("从最近列表移除"));

        QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
        if (chosen == openAction) {
            emit openRequested(item->data(kPathRole).toString());
        } else if (chosen == removeAction) {
            emit removeRequested(item->data(kPathRole).toString(),
                                 item->data(kSizeRole).toLongLong());
        }
    });

    connect(m_browseButton, &QPushButton::clicked, this, &StartPage::browseRequested);
}

void StartPage::setRecentBooks(const QVector<reader::storage::RecentBook> &books)
{
    m_list->clear();

    for (const reader::storage::RecentBook &book : books) {
        // 展示名取文件名（去扩展名）；真书名需解析后才能得到，此处不额外读取文件
        const QString name = QFileInfo(book.path).completeBaseName();
        const QString openedAt = book.lastOpened.isValid()
                                     ? book.lastOpened.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                     : tr("未知");
        const QString path = book.path;

        auto *item = new QListWidgetItem(name, m_list);
        item->setData(kPathRole, path);
        item->setData(kSizeRole, book.size);
        item->setToolTip(tr("%1\n最近打开：%2").arg(path, openedAt));
    }

    const bool empty = books.isEmpty();
    m_list->setVisible(!empty);
    m_emptyHint->setVisible(empty);
}

} // namespace reader::ui
