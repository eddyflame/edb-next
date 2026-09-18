#include "PatchManagerDialog.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <sstream>
#include <iomanip>

namespace edb_next {

static QString bytesToHex(const std::vector<uint8_t>& b) {
    std::ostringstream oss;
    for (size_t i = 0; i < b.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b[i]);
    }
    return QString::fromStdString(oss.str());
}

PatchManagerDialog::PatchManagerDialog(PatchManager& patchMgr, std::shared_ptr<DebugSession> session, QWidget* parent)
    : QDialog(parent), patchMgr_(patchMgr), session_(session)
{
    setupUi();
    connect(&patchMgr_, &PatchManager::patchesUpdated, this, &PatchManagerDialog::onRefreshTable);
    onRefreshTable();
}

void PatchManagerDialog::setupUi() {
    setWindowTitle("Patch Manager (Ctrl+P) - edb-next");
    resize(720, 420);

    auto* root_layout = new QVBoxLayout(this);

    auto* header = new QLabel("Active and Historical In-Memory Byte Patches:");
    root_layout->addWidget(header);

    tablePatches_ = new QTableWidget(this);
    tablePatches_->setColumnCount(6);
    tablePatches_->setHorizontalHeaderLabels({"#", "Address", "Original Bytes", "Patched Bytes", "Status", "Comment"});
    tablePatches_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tablePatches_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tablePatches_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tablePatches_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tablePatches_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tablePatches_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    tablePatches_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tablePatches_->setSelectionMode(QAbstractItemView::SingleSelection);
    tablePatches_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root_layout->addWidget(tablePatches_);

    auto* btn_layout = new QHBoxLayout();
    btnRevert_ = new QPushButton("Revert Selected", this);
    connect(btnRevert_, &QPushButton::clicked, this, &PatchManagerDialog::onRevertClicked);
    btnReapply_ = new QPushButton("Reapply Selected", this);
    connect(btnReapply_, &QPushButton::clicked, this, &PatchManagerDialog::onReapplyClicked);

    btnPatchFile_ = new QPushButton("💾 Patch File to Disk...", this);
    btnPatchFile_->setStyleSheet("font-weight: bold; padding: 6px; background-color: #2e6b4f; color: white;");
    connect(btnPatchFile_, &QPushButton::clicked, this, &PatchManagerDialog::onPatchFileClicked);

    auto* btn_close = new QPushButton("Close", this);
    connect(btn_close, &QPushButton::clicked, this, &QDialog::accept);

    btn_layout->addWidget(btnRevert_);
    btn_layout->addWidget(btnReapply_);
    btn_layout->addStretch();
    btn_layout->addWidget(btnPatchFile_);
    btn_layout->addWidget(btn_close);

    root_layout->addLayout(btn_layout);
}

void PatchManagerDialog::onRefreshTable() {
    tablePatches_->setRowCount(0);
    const auto& patches = patchMgr_.patches();

    for (size_t i = 0; i < patches.size(); ++i) {
        const auto& p = patches[i];
        int row = static_cast<int>(i);
        tablePatches_->insertRow(row);

        auto* idx_item = new QTableWidgetItem(QString::number(i + 1));
        tablePatches_->setItem(row, 0, idx_item);

        auto* addr_item = new QTableWidgetItem(p.address.toQString());
        tablePatches_->setItem(row, 1, addr_item);

        auto* orig_item = new QTableWidgetItem(bytesToHex(p.originalBytes));
        tablePatches_->setItem(row, 2, orig_item);

        auto* patch_item = new QTableWidgetItem(bytesToHex(p.patchedBytes));
        tablePatches_->setItem(row, 3, patch_item);

        auto* status_item = new QTableWidgetItem(p.isApplied ? "Applied" : "Reverted");
        status_item->setForeground(p.isApplied ? Qt::green : Qt::red);
        tablePatches_->setItem(row, 4, status_item);

        auto* cmt_item = new QTableWidgetItem(QString::fromStdString(p.comment));
        tablePatches_->setItem(row, 5, cmt_item);
    }
}

void PatchManagerDialog::onRevertClicked() {
    int row = tablePatches_->currentRow();
    if (row >= 0 && row < static_cast<int>(patchMgr_.patchCount())) {
        patchMgr_.revertPatch(static_cast<size_t>(row), session_.get());
    }
}

void PatchManagerDialog::onReapplyClicked() {
    int row = tablePatches_->currentRow();
    if (row >= 0 && row < static_cast<int>(patchMgr_.patchCount())) {
        patchMgr_.reapplyPatch(static_cast<size_t>(row), session_.get());
    }
}

void PatchManagerDialog::onPatchFileClicked() {
    if (patchMgr_.patchCount() == 0) {
        QMessageBox::information(this, "Patch Manager", "No patches have been created yet.");
        return;
    }

    QString original_binary;
    if (session_ && session_->pid() > 0) {
        char exe_buf[PATH_MAX] = {0};
        ssize_t len = ::readlink(("/proc/" + std::to_string(session_->pid()) + "/exe").c_str(), exe_buf, sizeof(exe_buf) - 1);
        if (len > 0) {
            original_binary = QString::fromUtf8(exe_buf);
        }
    }

    if (original_binary.isEmpty()) {
        original_binary = QFileDialog::getOpenFileName(this, "Select Original Binary to Patch", QString(), "All Files (*)");
        if (original_binary.isEmpty()) return;
    }

    QString default_out = original_binary + "_patched";
    QString output_binary = QFileDialog::getSaveFileName(this, "Save Patched Binary As", default_out, "All Files (*)");
    if (output_binary.isEmpty()) return;

    std::string err;
    Address baseAddr = session_ ? session_->baseAddress() : Address(0);
    bool ok = patchMgr_.patchFileToDisk(original_binary.toStdString(), output_binary.toStdString(), err, baseAddr);
    if (!ok) {
        QMessageBox::critical(this, "Patching Error", QString("Failed to create patched binary:\n%1").arg(QString::fromStdString(err)));
    } else {
        QMessageBox::information(this, "Patch Successful",
            QString("Successfully patched %1 modification(s) into file:\n%2\n\nThe patched executable is ready to run!")
            .arg(patchMgr_.patchCount())
            .arg(output_binary));
    }
}

} // namespace edb_next
