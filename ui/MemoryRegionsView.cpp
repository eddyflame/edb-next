#include "MemoryRegionsView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QFontDatabase>
#include <QColor>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <iomanip>
#include <sstream>

namespace edb_next {

static QString formatSize(uint64_t bytes) {
    if (bytes >= 1024 * 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
    }
    if (bytes >= 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
    }
    if (bytes >= 1024) {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    return QString::number(bytes) + " B";
}

MemoryRegionsView::MemoryRegionsView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void MemoryRegionsView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Filter bar
    auto* top_bar = new QHBoxLayout();
    top_bar->setContentsMargins(2, 2, 2, 2);
    auto* lbl_filter = new QLabel("Filter:", this);
    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Filter by pathname, permission, or address...");
    filterEdit_->setClearButtonEnabled(true);
    connect(filterEdit_, &QLineEdit::textChanged, this, &MemoryRegionsView::handleFilterChanged);

    top_bar->addWidget(lbl_filter);
    top_bar->addWidget(filterEdit_);
    layout->addLayout(top_bar);

    // Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"Start Address", "End Address", "Size", "Permissions", "Offset", "Module / Path"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);

    QFont mono_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono_font.setPointSize(9);
    table_->setFont(mono_font);

    connect(table_, &QTableWidget::cellDoubleClicked, this, &MemoryRegionsView::handleCellDoubleClicked);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &MemoryRegionsView::handleCustomContextMenu);
    layout->addWidget(table_);
}

void MemoryRegionsView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void MemoryRegionsView::refresh() {
    dirty_ = false;
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        allRegions_.clear();
        displayedRegions_.clear();
        table_->setRowCount(0);
        return;
    }

    allRegions_ = session->memoryRegions();
    handleFilterChanged(filterEdit_->text());
}

void MemoryRegionsView::handleFilterChanged(const QString& text) {
    displayedRegions_.clear();
    QString filter = text.trimmed();

    for (const auto& r : allRegions_) {
        if (filter.isEmpty()) {
            displayedRegions_.push_back(r);
        } else {
            QString path = QString::fromStdString(r.pathname);
            QString perms = QString::fromStdString(r.permissions);
            QString start = QString::fromStdString(r.start.toHex());
            if (path.contains(filter, Qt::CaseInsensitive) ||
                perms.contains(filter, Qt::CaseInsensitive) ||
                start.contains(filter, Qt::CaseInsensitive)) {
                displayedRegions_.push_back(r);
            }
        }
    }

    renderTable();
}

void MemoryRegionsView::renderTable() {
    table_->setRowCount(static_cast<int>(displayedRegions_.size()));

    for (int r = 0; r < static_cast<int>(displayedRegions_.size()); ++r) {
        const auto& reg = displayedRegions_[r];

        // 0: Start
        auto* item_start = new QTableWidgetItem(QString::fromStdString(reg.start.toHex()));
        item_start->setForeground(QColor(100, 180, 240));

        // 1: End
        auto* item_end = new QTableWidgetItem(QString::fromStdString(reg.end.toHex()));

        // 2: Size
        auto* item_size = new QTableWidgetItem(formatSize(reg.size()));
        item_size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // 3: Permissions
        auto* item_perms = new QTableWidgetItem(QString::fromStdString(reg.permissions));
        item_perms->setTextAlignment(Qt::AlignCenter);
        if (reg.isExecutable()) {
            item_perms->setForeground(QColor(230, 80, 80)); // Red for Executable
            QFont bold_font = table_->font();
            bold_font.setBold(true);
            item_perms->setFont(bold_font);
        } else if (reg.isWritable()) {
            item_perms->setForeground(QColor(240, 180, 60)); // Orange for Writable
        } else {
            item_perms->setForeground(QColor(160, 160, 160));
        }

        // 4: Offset
        std::ostringstream off_oss;
        off_oss << "0x" << std::hex << reg.offset;
        auto* item_off = new QTableWidgetItem(QString::fromStdString(off_oss.str()));
        item_off->setForeground(QColor(120, 120, 120));

        // 5: Pathname
        QString display_path = QString::fromStdString(reg.pathname);
        if (display_path.isEmpty()) {
            display_path = "[anonymous]";
        }
        auto* item_path = new QTableWidgetItem(display_path);
        if (reg.pathname.find("stack") != std::string::npos) {
            item_path->setForeground(QColor(140, 200, 140));
        } else if (reg.pathname.find("heap") != std::string::npos) {
            item_path->setForeground(QColor(200, 160, 220));
        }

        table_->setItem(r, 0, item_start);
        table_->setItem(r, 1, item_end);
        table_->setItem(r, 2, item_size);
        table_->setItem(r, 3, item_perms);
        table_->setItem(r, 4, item_off);
        table_->setItem(r, 5, item_path);
    }
}

void MemoryRegionsView::handleCellDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    if (row >= 0 && row < static_cast<int>(displayedRegions_.size())) {
        const auto& reg = displayedRegions_[row];
        Q_EMIT jumpToAddressRequested(reg.start, reg.isExecutable());
    }
}

void MemoryRegionsView::handleCustomContextMenu(const QPoint& pos) {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedRegions_.size())) return;

    const auto& reg = displayedRegions_[row];

    QMenu menu(this);
    menu.addAction("Follow in Disassembly", [this, reg]() {
        Q_EMIT jumpToAddressRequested(reg.start, true);
    });

    menu.addAction("Follow in Dump (Hex View)", [this, reg]() {
        Q_EMIT jumpToAddressRequested(reg.start, false);
    });

    menu.addSeparator();

    menu.addAction("Change Page Permissions (mprotect)...", this, &MemoryRegionsView::changePermissionsPrompt);
    menu.addAction("Allocate Target Memory (mmap)...", this, &MemoryRegionsView::allocateMemoryPrompt);
    menu.addAction("Free Target Memory (munmap)...", this, &MemoryRegionsView::freeMemoryPrompt);

    menu.addSeparator();

    menu.addAction("Dump Region to File (*.bin)...", this, &MemoryRegionsView::dumpSelectedRegion);

    menu.addSeparator();

    menu.addAction("Copy Start Address", [reg]() {
        QApplication::clipboard()->setText(QString::fromStdString(reg.start.toHex()));
    });

    menu.addAction("Copy Size", [reg]() {
        QApplication::clipboard()->setText(formatSize(reg.size()));
    });

    menu.addSeparator();
    menu.addAction("Refresh", this, &MemoryRegionsView::refresh);

    menu.exec(table_->mapToGlobal(pos));
}

void MemoryRegionsView::dumpSelectedRegion() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedRegions_.size())) return;

    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        QMessageBox::warning(this, "Dump Error", "Target must be running or paused to dump memory.");
        return;
    }

    const auto& reg = displayedRegions_[row];
    QString default_name = QString("dump_%1_%2.bin")
        .arg(QString::fromStdString(reg.start.toHex()))
        .arg(reg.size());

    QString path = QFileDialog::getSaveFileName(this, "Dump Memory Region to Binary File", default_name, "Binary Files (*.bin);;All Files (*)");
    if (path.isEmpty()) return;

    bool ok = session->dumpMemoryToFile(reg.start, reg.size(), path.toStdString());
    if (ok) {
        QMessageBox::information(this, "Dump Successful", QString("Successfully dumped %1 bytes to:\n%2").arg(reg.size()).arg(path));
    } else {
        QMessageBox::critical(this, "Dump Error", "Failed to dump memory region. Verify read permissions and disk space.");
    }
}

void MemoryRegionsView::changePermissionsPrompt() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedRegions_.size())) return;

    auto session = session_.lock();
    if (!session || session->state() != SessionState::Paused) {
        QMessageBox::warning(this, "Permissions Error", "Target process must be paused to modify memory protections.");
        return;
    }

    const auto& reg = displayedRegions_[row];

    QDialog dlg(this);
    dlg.setWindowTitle("Change Page Permissions (mprotect)");
    auto* lyt = new QVBoxLayout(&dlg);

    auto* infoLbl = new QLabel(QString("Region: %1 - %2\nCurrent: %3")
                                   .arg(QString::fromStdString(reg.start.toHex()))
                                   .arg(QString::fromStdString(reg.end.toHex()))
                                   .arg(QString::fromStdString(reg.permissions)), &dlg);
    lyt->addWidget(infoLbl);

    auto* chkRead = new QCheckBox("Read (PROT_READ)", &dlg);
    chkRead->setChecked(reg.isReadable());
    lyt->addWidget(chkRead);

    auto* chkWrite = new QCheckBox("Write (PROT_WRITE)", &dlg);
    chkWrite->setChecked(reg.isWritable());
    lyt->addWidget(chkWrite);

    auto* chkExec = new QCheckBox("Execute (PROT_EXEC)", &dlg);
    chkExec->setChecked(reg.isExecutable());
    lyt->addWidget(chkExec);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lyt->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        int prot = 0;
        if (chkRead->isChecked())  prot |= 1; // PROT_READ
        if (chkWrite->isChecked()) prot |= 2; // PROT_WRITE
        if (chkExec->isChecked())  prot |= 4; // PROT_EXEC

        if (session->changeMemoryProtection(reg.start, reg.size(), prot)) {
            QMessageBox::information(this, "mprotect Success", "Successfully updated memory page permissions.");
            refresh();
        } else {
            QMessageBox::critical(this, "mprotect Failed", "Kernel rejected mprotect call on target region.");
        }
    }
}

void MemoryRegionsView::allocateMemoryPrompt() {
    auto session = session_.lock();
    if (!session || session->state() != SessionState::Paused) {
        QMessageBox::warning(this, "mmap Error", "Target process must be paused to allocate memory.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Allocate Target Memory (mmap)");
    auto* lyt = new QVBoxLayout(&dlg);

    auto* lblSize = new QLabel("Allocation Size (Bytes):", &dlg);
    lyt->addWidget(lblSize);

    auto* spSize = new QSpinBox(&dlg);
    spSize->setRange(4096, 1024 * 1024 * 128); // 4KB to 128MB
    spSize->setSingleStep(4096);
    spSize->setValue(4096);
    lyt->addWidget(spSize);

    auto* chkRead = new QCheckBox("Read (PROT_READ)", &dlg);
    chkRead->setChecked(true);
    lyt->addWidget(chkRead);

    auto* chkWrite = new QCheckBox("Write (PROT_WRITE)", &dlg);
    chkWrite->setChecked(true);
    lyt->addWidget(chkWrite);

    auto* chkExec = new QCheckBox("Execute (PROT_EXEC)", &dlg);
    chkExec->setChecked(true);
    lyt->addWidget(chkExec);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lyt->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        int prot = 0;
        if (chkRead->isChecked())  prot |= 1;
        if (chkWrite->isChecked()) prot |= 2;
        if (chkExec->isChecked())  prot |= 4;

        auto addr = session->allocateMemory(spSize->value(), prot);
        if (addr.has_value()) {
            QMessageBox::information(this, "mmap Success",
                QString("Successfully allocated %1 bytes at %2")
                    .arg(spSize->value())
                    .arg(QString::fromStdString(addr->toHex())));
            refresh();
        } else {
            QMessageBox::critical(this, "mmap Failed", "Failed to allocate memory in target process.");
        }
    }
}

void MemoryRegionsView::freeMemoryPrompt() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedRegions_.size())) return;

    auto session = session_.lock();
    if (!session || session->state() != SessionState::Paused) {
        QMessageBox::warning(this, "munmap Error", "Target process must be paused to free memory.");
        return;
    }

    const auto& reg = displayedRegions_[row];
    auto rep = QMessageBox::question(this, "Confirm munmap",
        QString("Are you sure you want to unmap region:\n%1 - %2 (%3)?")
            .arg(QString::fromStdString(reg.start.toHex()))
            .arg(QString::fromStdString(reg.end.toHex()))
            .arg(formatSize(reg.size())),
        QMessageBox::Yes | QMessageBox::No);

    if (rep == QMessageBox::Yes) {
        if (session->freeMemory(reg.start, reg.size())) {
            QMessageBox::information(this, "munmap Success", "Memory region successfully unmapped.");
            refresh();
        } else {
            QMessageBox::critical(this, "munmap Failed", "Kernel failed to unmap target region.");
        }
    }
}

} // namespace edb_next
