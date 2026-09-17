#include "ThreadsView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>

namespace edb_next {

ThreadsView::ThreadsView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ThreadsView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* toolLayout = new QHBoxLayout();
    btnRefresh_ = new QPushButton("⟳ Refresh", this);
    btnSwitch_ = new QPushButton("Switch Thread", this);
    btnFreezeThaw_ = new QPushButton("❄ Freeze / Thaw", this);
    btnFreezeAll_ = new QPushButton("❄ Freeze Others", this);
    btnThawAll_ = new QPushButton("🔥 Thaw All", this);
    statusLabel_ = new QLabel("Active Threads: 0", this);

    toolLayout->addWidget(btnRefresh_);
    toolLayout->addWidget(btnSwitch_);
    toolLayout->addWidget(btnFreezeThaw_);
    toolLayout->addWidget(btnFreezeAll_);
    toolLayout->addWidget(btnThawAll_);
    toolLayout->addWidget(statusLabel_);
    toolLayout->addStretch();
    layout->addLayout(toolLayout);

    table_ = new QTableWidget(this);
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({"TID", "Thread Name", "State", "Frozen", "Current RIP", "Function Symbol", "RSP", "Active"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addWidget(table_);

    connect(btnRefresh_, &QPushButton::clicked, this, &ThreadsView::refresh);
    connect(btnSwitch_, &QPushButton::clicked, this, &ThreadsView::onSwitchThreadClicked);
    connect(btnFreezeThaw_, &QPushButton::clicked, this, &ThreadsView::onFreezeThawClicked);
    connect(btnFreezeAll_, &QPushButton::clicked, this, &ThreadsView::onFreezeAllClicked);
    connect(btnThawAll_, &QPushButton::clicked, this, &ThreadsView::onThawAllClicked);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &ThreadsView::onCellDoubleClicked);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &ThreadsView::onCustomContextMenu);
}

void ThreadsView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    if (session) {
        connect(session.get(), &DebugSession::threadFreezeStateChanged, this, &ThreadsView::refresh, Qt::UniqueConnection);
    }
    refresh();
}

void ThreadsView::refresh() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        table_->setRowCount(0);
        statusLabel_->setText("Active Threads: 0");
        currentThreads_.clear();
        return;
    }

    currentThreads_ = session->getThreads();
    table_->setRowCount(static_cast<int>(currentThreads_.size()));

    size_t frozenCount = 0;
    for (const auto& t : currentThreads_) {
        if (t.isFrozen) frozenCount++;
    }

    QString statusText = QString("Active Threads: %1 (Current TID: %2)")
        .arg(currentThreads_.size()).arg(session->activeTid());
    if (frozenCount > 0) {
        statusText += QString(" [❄ %1 Frozen]").arg(frozenCount);
    }
    statusLabel_->setText(statusText);

    for (int r = 0; r < static_cast<int>(currentThreads_.size()); ++r) {
        const auto& t = currentThreads_[r];

        auto* itemTid = new QTableWidgetItem(QString::number(t.tid));
        auto* itemName = new QTableWidgetItem(QString::fromStdString(t.name));
        auto* itemState = new QTableWidgetItem(QString::fromStdString(t.state));
        auto* itemFrozen = new QTableWidgetItem(t.isFrozen ? "❄ FROZEN" : "-");
        auto* itemRip = new QTableWidgetItem(QString::fromStdString(t.rip.toHex()));
        auto* itemSym = new QTableWidgetItem(QString::fromStdString(t.symbol));
        auto* itemRsp = new QTableWidgetItem(QString::fromStdString(t.rsp.toHex()));
        auto* itemActive = new QTableWidgetItem(t.isActive ? "➔ ACTIVE" : "");

        itemTid->setTextAlignment(Qt::AlignCenter);
        itemState->setTextAlignment(Qt::AlignCenter);
        itemFrozen->setTextAlignment(Qt::AlignCenter);
        itemActive->setTextAlignment(Qt::AlignCenter);

        if (t.isFrozen) {
            itemFrozen->setForeground(QColor(100, 200, 255));
            QColor frozenBg(30, 70, 110, 80);
            itemFrozen->setBackground(frozenBg);
            QFont f = itemFrozen->font();
            f.setBold(true);
            itemFrozen->setFont(f);
        }

        if (t.isActive) {
            QColor activeBg(60, 100, 70, 70);
            itemTid->setBackground(activeBg);
            itemName->setBackground(activeBg);
            itemState->setBackground(activeBg);
            itemRip->setBackground(activeBg);
            itemSym->setBackground(activeBg);
            itemRsp->setBackground(activeBg);
            itemActive->setBackground(activeBg);

            itemActive->setForeground(QColor(80, 220, 140));
            QFont boldFont = itemActive->font();
            boldFont.setBold(true);
            itemActive->setFont(boldFont);
        }

        table_->setItem(r, 0, itemTid);
        table_->setItem(r, 1, itemName);
        table_->setItem(r, 2, itemState);
        table_->setItem(r, 3, itemFrozen);
        table_->setItem(r, 4, itemRip);
        table_->setItem(r, 5, itemSym);
        table_->setItem(r, 6, itemRsp);
        table_->setItem(r, 7, itemActive);
    }
}

void ThreadsView::onCellDoubleClicked(int row, int column) {
    if (row < 0 || row >= static_cast<int>(currentThreads_.size())) return;
    const auto& t = currentThreads_[row];

    if (column == 4 && !t.rip.isNull()) { // Double clicked RIP column: jump to address
        Q_EMIT jumpToAddressRequested(t.rip);
        return;
    }

    if (auto session = session_.lock()) {
        session->switchThread(t.tid);
        refresh();
        Q_EMIT threadSwitched(t.tid);
        if (!t.rip.isNull()) {
            Q_EMIT jumpToAddressRequested(t.rip);
        }
    }
}

void ThreadsView::onSwitchThreadClicked() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentThreads_.size())) return;
    const auto& t = currentThreads_[row];

    if (auto session = session_.lock()) {
        session->switchThread(t.tid);
        refresh();
        Q_EMIT threadSwitched(t.tid);
        if (!t.rip.isNull()) {
            Q_EMIT jumpToAddressRequested(t.rip);
        }
    }
}

void ThreadsView::onFreezeThawClicked() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentThreads_.size())) return;
    const auto& t = currentThreads_[row];

    if (auto session = session_.lock()) {
        if (t.isFrozen) {
            session->thawThread(t.tid);
        } else {
            session->freezeThread(t.tid);
        }
        refresh();
    }
}

void ThreadsView::onFreezeAllClicked() {
    if (auto session = session_.lock()) {
        session->freezeAllOtherThreads();
        refresh();
    }
}

void ThreadsView::onThawAllClicked() {
    if (auto session = session_.lock()) {
        session->thawAllThreads();
        refresh();
    }
}

void ThreadsView::onCustomContextMenu(const QPoint& pos) {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentThreads_.size())) return;
    const auto& t = currentThreads_[row];

    QMenu menu(this);
    auto* actSwitch = menu.addAction(QString("Switch to Thread %1 (%2)").arg(t.tid).arg(QString::fromStdString(t.name)));
    auto* actJump = menu.addAction("Jump to Thread RIP in Disassembly");
    menu.addSeparator();

    QAction* actFreezeThaw = nullptr;
    if (t.isFrozen) {
        actFreezeThaw = menu.addAction(QString("🔥 Thaw Thread %1").arg(t.tid));
    } else {
        actFreezeThaw = menu.addAction(QString("❄ Freeze Thread %1").arg(t.tid));
    }
    auto* actFreezeAll = menu.addAction("❄ Freeze All Other Threads");
    auto* actThawAll = menu.addAction("🔥 Thaw All Threads");

    menu.addSeparator();
    auto* actRefresh = menu.addAction("Refresh Threads");

    auto* selected = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (selected == actSwitch) {
        onSwitchThreadClicked();
    } else if (selected == actJump && !t.rip.isNull()) {
        Q_EMIT jumpToAddressRequested(t.rip);
    } else if (selected == actFreezeThaw) {
        onFreezeThawClicked();
    } else if (selected == actFreezeAll) {
        onFreezeAllClicked();
    } else if (selected == actThawAll) {
        onThawAllClicked();
    } else if (selected == actRefresh) {
        refresh();
    }
}

} // namespace edb_next
