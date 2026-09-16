#include "WatchView.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <sstream>
#include <iomanip>

namespace edb_next {

WatchView::WatchView(QWidget* parent) : QWidget(parent) {
    setupUi();

    // Add some handy default watches
    addWatch("rax");
    addWatch("rdi");
    addWatch("[rsp]");
}

void WatchView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void WatchView::setupUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(4, 4, 4, 4);

    auto* top_layout = new QHBoxLayout();
    txtNewExpr_ = new QLineEdit(this);
    txtNewExpr_->setPlaceholderText("Enter expression to watch (e.g. 'rax', '[rbp - 8]', 'rsp + 16')...");
    connect(txtNewExpr_, &QLineEdit::returnPressed, this, &WatchView::onAddClicked);

    btnAdd_ = new QPushButton("Add Watch", this);
    connect(btnAdd_, &QPushButton::clicked, this, &WatchView::onAddClicked);
    btnRemove_ = new QPushButton("Remove", this);
    connect(btnRemove_, &QPushButton::clicked, this, &WatchView::onRemoveClicked);
    btnClear_ = new QPushButton("Clear All", this);
    connect(btnClear_, &QPushButton::clicked, this, &WatchView::onClearClicked);

    top_layout->addWidget(new QLabel("Expression:"));
    top_layout->addWidget(txtNewExpr_, 1);
    top_layout->addWidget(btnAdd_);
    top_layout->addWidget(btnRemove_);
    top_layout->addWidget(btnClear_);
    root_layout->addLayout(top_layout);

    tableWatches_ = new QTableWidget(this);
    tableWatches_->setColumnCount(4);
    tableWatches_->setHorizontalHeaderLabels({"Expression", "Hex Value", "Decimal", "String / Preview"});
    tableWatches_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWatches_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableWatches_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableWatches_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    tableWatches_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWatches_->setFont(QFont("Monospace", 9));
    root_layout->addWidget(tableWatches_);
}

void WatchView::addWatch(const std::string& expr) {
    if (expr.empty()) return;
    for (const auto& w : watches_) {
        if (w.expression == expr) return;
    }
    watches_.push_back(WatchItem{.expression = expr, .lastValue = std::nullopt, .hasChanged = false});
    refresh();
}

void WatchView::onAddClicked() {
    QString raw = txtNewExpr_->text().trimmed();
    if (raw.isEmpty()) return;
    txtNewExpr_->clear();
    addWatch(raw.toStdString());
}

void WatchView::onRemoveClicked() {
    int row = tableWatches_->currentRow();
    if (row >= 0 && row < static_cast<int>(watches_.size())) {
        watches_.erase(watches_.begin() + row);
        refresh();
    }
}

void WatchView::onClearClicked() {
    watches_.clear();
    refresh();
}

void WatchView::refresh() {
    tableWatches_->setRowCount(0);
    if (!session_) return;

    const auto& regs = session_->registers();

    for (size_t i = 0; i < watches_.size(); ++i) {
        auto& item = watches_[i];
        int row = static_cast<int>(i);
        tableWatches_->insertRow(row);

        auto* expr_item = new QTableWidgetItem(QString::fromStdString(item.expression));
        tableWatches_->setItem(row, 0, expr_item);

        auto opt_val = ExpressionEvaluator::evaluate(item.expression, regs, nullptr);
        if (opt_val.has_value()) {
            uint64_t val = *opt_val;
            if (item.lastValue.has_value() && *item.lastValue != val) {
                item.hasChanged = true;
            } else {
                item.hasChanged = false;
            }
            item.lastValue = val;

            std::ostringstream oss;
            oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << val;
            auto* hex_item = new QTableWidgetItem(QString::fromStdString(oss.str()));
            auto* dec_item = new QTableWidgetItem(QString::number(val));

            if (item.hasChanged) {
                hex_item->setForeground(QColor(255, 100, 100)); // Red/amber for changed values
                dec_item->setForeground(QColor(255, 100, 100));
            } else {
                hex_item->setForeground(QColor(100, 220, 100));
            }

            tableWatches_->setItem(row, 1, hex_item);
            tableWatches_->setItem(row, 2, dec_item);

            // Preview string if value points to readable ASCII
            std::string preview;
            if (val > 0x1000) {
                std::vector<uint8_t> mem = session_->readMemory(Address(val), 32);
                if (!mem.empty()) {
                    bool is_printable = true;
                    std::string str_buf;
                    for (uint8_t c : mem) {
                        if (c == 0) break;
                        if (c >= 32 && c <= 126) {
                            str_buf.push_back(static_cast<char>(c));
                        } else {
                            is_printable = false;
                            break;
                        }
                    }
                    if (is_printable && str_buf.length() >= 3) {
                        preview = "\"" + str_buf + "\"";
                    }
                }
            }

            auto* prev_item = new QTableWidgetItem(QString::fromStdString(preview));
            prev_item->setForeground(QColor(200, 200, 200));
            tableWatches_->setItem(row, 3, prev_item);
        } else {
            auto* err_item = new QTableWidgetItem("<Evaluation Failed>");
            err_item->setForeground(Qt::gray);
            tableWatches_->setItem(row, 1, err_item);
            tableWatches_->setItem(row, 2, new QTableWidgetItem("-"));
            tableWatches_->setItem(row, 3, new QTableWidgetItem("-"));
        }
    }
}

} // namespace edb_next
