#include "IntermodularCallsView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <QColor>

namespace edb_next {

IntermodularCallsView::IntermodularCallsView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void IntermodularCallsView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Top control bar
    auto* top_bar = new QHBoxLayout();
    top_bar->setContentsMargins(2, 2, 2, 2);

    scanBtn_ = new QPushButton("Scan Intermodular Calls", this);
    scanBtn_->setStyleSheet("background-color: #2e6b38; color: white; font-weight: bold;");
    connect(scanBtn_, &QPushButton::clicked, this, &IntermodularCallsView::handleScanClicked);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Filter by API name (e.g. puts, malloc, free, printf)...");
    filterEdit_->setClearButtonEnabled(true);
    connect(filterEdit_, &QLineEdit::textChanged, this, &IntermodularCallsView::handleFilterChanged);

    statusLabel_ = new QLabel("Ready. Click Scan to find library calls.", this);
    statusLabel_->setStyleSheet("color: #888888;");

    top_bar->addWidget(scanBtn_);
    top_bar->addWidget(new QLabel("Filter:", this));
    top_bar->addWidget(filterEdit_, 1);
    top_bar->addWidget(statusLabel_);

    layout->addLayout(top_bar);

    // Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Call Address", "Caller Function", "Target API", "Library", "Instruction"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);

    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(9);
    table_->setFont(mono);

    connect(table_, &QTableWidget::cellDoubleClicked, this, &IntermodularCallsView::handleCellDoubleClicked);
    layout->addWidget(table_);
}

void IntermodularCallsView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void IntermodularCallsView::refresh() {
    dirty_ = false;
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        allCalls_.clear();
        displayedCalls_.clear();
        table_->setRowCount(0);
        statusLabel_->setText("No active debug session.");
        return;
    }
}

void IntermodularCallsView::handleScanClicked() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        statusLabel_->setText("Target must be running or paused to scan.");
        return;
    }

    allCalls_ = IntermodularCallsFinder::findCalls(session);
    statusLabel_->setText(QString("Found %1 intermodular call(s).").arg(allCalls_.size()));
    handleFilterChanged(filterEdit_->text());
}

void IntermodularCallsView::handleFilterChanged(const QString& text) {
    displayedCalls_.clear();
    QString filter = text.trimmed();

    for (const auto& call : allCalls_) {
        if (filter.isEmpty()) {
            displayedCalls_.push_back(call);
        } else {
            QString api = QString::fromStdString(call.calleeApi);
            QString caller = QString::fromStdString(call.callerFunction);
            QString insn = QString::fromStdString(call.instruction);
            if (api.contains(filter, Qt::CaseInsensitive) ||
                caller.contains(filter, Qt::CaseInsensitive) ||
                insn.contains(filter, Qt::CaseInsensitive)) {
                displayedCalls_.push_back(call);
            }
        }
    }

    renderTable();
}

void IntermodularCallsView::renderTable() {
    table_->setRowCount(static_cast<int>(displayedCalls_.size()));

    for (int r = 0; r < static_cast<int>(displayedCalls_.size()); ++r) {
        const auto& call = displayedCalls_[r];

        auto* item_addr = new QTableWidgetItem(call.callAddress.toQString());
        item_addr->setForeground(QColor(100, 180, 240));

        auto* item_caller = new QTableWidgetItem(QString::fromStdString(call.callerFunction));
        item_caller->setForeground(QColor(200, 200, 200));

        auto* item_api = new QTableWidgetItem(QString::fromStdString(call.calleeApi));
        item_api->setForeground(QColor(240, 100, 100)); // Red for API target
        QFont bold_font = table_->font();
        bold_font.setBold(true);
        item_api->setFont(bold_font);

        auto* item_lib = new QTableWidgetItem(QString::fromStdString(call.library));
        item_lib->setForeground(QColor(140, 140, 140));

        auto* item_insn = new QTableWidgetItem(QString::fromStdString(call.instruction));
        item_insn->setForeground(QColor(180, 220, 180));

        table_->setItem(r, 0, item_addr);
        table_->setItem(r, 1, item_caller);
        table_->setItem(r, 2, item_api);
        table_->setItem(r, 3, item_lib);
        table_->setItem(r, 4, item_insn);
    }
}

void IntermodularCallsView::handleCellDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    if (row >= 0 && row < static_cast<int>(displayedCalls_.size())) {
        const auto& call = displayedCalls_[row];
        Q_EMIT jumpToAddressRequested(call.callAddress, true);
    }
}

} // namespace edb_next
