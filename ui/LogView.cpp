#include "LogView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QFontDatabase>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QMenu>
#include <fstream>

namespace edb_next {

LogView::LogView(QWidget* parent) : QWidget(parent) {
    setupUi();

    LogManager::instance().setCallback([this](const LogEntry& entry) {
        QMetaObject::invokeMethod(this, [this, entry]() {
            appendLogEntry(entry);
        }, Qt::QueuedConnection);
    });

    reloadAllEntries();
}

LogView::~LogView() {
    LogManager::instance().setCallback(nullptr);
}

void LogView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Filter controls
    auto* top_bar = new QHBoxLayout();
    top_bar->setContentsMargins(2, 2, 2, 2);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("Filter logs by text or category...");
    searchEdit_->setClearButtonEnabled(true);
    connect(searchEdit_, &QLineEdit::textChanged, this, &LogView::handleFilterChanged);

    levelFilterCombo_ = new QComboBox(this);
    levelFilterCombo_->addItems({"All Levels", "INFO", "DEBUG", "EVENT", "BP", "TRACE", "PLUGIN", "CMD", "ERROR"});
    connect(levelFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogView::handleFilterChanged);

    autoScrollCheck_ = new QCheckBox("Auto Scroll", this);
    autoScrollCheck_->setChecked(true);

    clearBtn_ = new QPushButton("Clear", this);
    connect(clearBtn_, &QPushButton::clicked, this, &LogView::clearLog);

    exportBtn_ = new QPushButton("Export...", this);
    connect(exportBtn_, &QPushButton::clicked, this, &LogView::exportLogToFile);

    top_bar->addWidget(new QLabel("Filter:", this));
    top_bar->addWidget(searchEdit_, 1);
    top_bar->addWidget(new QLabel("Level:", this));
    top_bar->addWidget(levelFilterCombo_);
    top_bar->addWidget(autoScrollCheck_);
    top_bar->addWidget(clearBtn_);
    top_bar->addWidget(exportBtn_);

    layout->addLayout(top_bar);

    // Table view
    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Time", "Level", "Category", "Message"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);

    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(9);
    table_->setFont(mono);

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &LogView::handleCustomContextMenu);

    layout->addWidget(table_);
}

void LogView::appendLogEntry(const LogEntry& entry) {
    // Check level filter
    QString level_filter = levelFilterCombo_->currentText();
    if (level_filter != "All Levels" && QString::fromStdString(entry.levelString()) != level_filter) {
        return;
    }

    // Check search filter
    QString search_text = searchEdit_->text().trimmed();
    if (!search_text.isEmpty()) {
        QString cat = QString::fromStdString(entry.category);
        QString msg = QString::fromStdString(entry.message);
        if (!cat.contains(search_text, Qt::CaseInsensitive) && !msg.contains(search_text, Qt::CaseInsensitive)) {
            return;
        }
    }

    int row = table_->rowCount();
    table_->insertRow(row);

    auto* item_time = new QTableWidgetItem(QString::fromStdString(entry.formatTime()));
    item_time->setForeground(QColor(120, 120, 120));

    auto* item_level = new QTableWidgetItem(QString::fromStdString(entry.levelString()));
    item_level->setTextAlignment(Qt::AlignCenter);

    QColor lvl_color(180, 180, 180);
    switch (entry.level) {
        case LogLevel::Info:       lvl_color = QColor(80, 170, 240); break;
        case LogLevel::Event:      lvl_color = QColor(100, 220, 120); break;
        case LogLevel::Breakpoint: lvl_color = QColor(240, 90, 90); break;
        case LogLevel::Trace:      lvl_color = QColor(200, 140, 240); break;
        case LogLevel::Plugin:     lvl_color = QColor(100, 220, 220); break;
        case LogLevel::Command:    lvl_color = QColor(240, 200, 80); break;
        case LogLevel::Warning:    lvl_color = QColor(240, 160, 60); break;
        case LogLevel::Error:      lvl_color = QColor(255, 60, 60); break;
        default: break;
    }
    item_level->setForeground(lvl_color);

    auto* item_cat = new QTableWidgetItem(QString::fromStdString(entry.category));
    item_cat->setForeground(QColor(160, 180, 200));

    auto* item_msg = new QTableWidgetItem(QString::fromStdString(entry.message));
    item_msg->setForeground(QColor(220, 220, 220));

    table_->setItem(row, 0, item_time);
    table_->setItem(row, 1, item_level);
    table_->setItem(row, 2, item_cat);
    table_->setItem(row, 3, item_msg);

    if (autoScrollCheck_->isChecked()) {
        table_->scrollToBottom();
    }
}

void LogView::reloadAllEntries() {
    table_->setRowCount(0);
    auto entries = LogManager::instance().entries();
    for (const auto& e : entries) {
        appendLogEntry(e);
    }
}

void LogView::clearLog() {
    LogManager::instance().clear();
    table_->setRowCount(0);
}

void LogView::exportLogToFile() {
    QString path = QFileDialog::getSaveFileName(this, "Export Log", "edb_debug.log", "Log Files (*.log *.txt)");
    if (path.isEmpty()) return;

    std::ofstream out(path.toStdString());
    if (!out.is_open()) {
        QMessageBox::warning(this, "Export Error", "Failed to open file for writing: " + path);
        return;
    }

    auto entries = LogManager::instance().entries();
    for (const auto& e : entries) {
        out << "[" << e.formatTime() << "] [" << e.levelString() << "] [" << e.category << "] " << e.message << "\n";
    }

    QMessageBox::information(this, "Export Complete", QString("Exported %1 log entries to %2").arg(entries.size()).arg(path));
}

void LogView::handleFilterChanged() {
    reloadAllEntries();
}

void LogView::handleCustomContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.addAction("Copy Row", [this]() {
        int r = table_->currentRow();
        if (r >= 0) {
            QString line = QString("[%1] [%2] [%3] %4")
                .arg(table_->item(r, 0)->text())
                .arg(table_->item(r, 1)->text())
                .arg(table_->item(r, 2)->text())
                .arg(table_->item(r, 3)->text());
            QApplication::clipboard()->setText(line);
        }
    });

    menu.addAction("Copy Message", [this]() {
        int r = table_->currentRow();
        if (r >= 0 && table_->item(r, 3)) {
            QApplication::clipboard()->setText(table_->item(r, 3)->text());
        }
    });

    menu.addSeparator();
    menu.addAction("Clear Log", this, &LogView::clearLog);
    menu.addAction("Export Log to File...", this, &LogView::exportLogToFile);

    menu.exec(table_->mapToGlobal(pos));
}

} // namespace edb_next
