#include "AssembleDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontDatabase>
#include <QMessageBox>

namespace edb_next {

AssembleDialog::AssembleDialog(std::shared_ptr<DebugSession> session,
                               Address targetAddr,
                               size_t originalLength,
                               const QString& defaultAsm,
                               QWidget* parent)
    : QDialog(parent),
      session_(std::move(session)),
      currentAddr_(targetAddr),
      origLen_(originalLength) {
    setupUi();
    setTargetInstruction(targetAddr, originalLength, defaultAsm);
}

void AssembleDialog::setupUi() {
    setWindowTitle("Assemble Instruction (Space)");
    setMinimumWidth(440);
    setModal(false);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    lblAddress_ = new QLabel(this);
    lblAddress_->setTextFormat(Qt::RichText);
    mainLayout->addWidget(lblAddress_);

    txtAsm_ = new QLineEdit(this);
    txtAsm_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    txtAsm_->setPlaceholderText("e.g. mov eax, 1 / nop / jmp 0x401050");
    mainLayout->addWidget(txtAsm_);

    auto* optionsLayout = new QHBoxLayout();
    chkFillNops_ = new QCheckBox("Fill with NOPs", this);
    chkFillNops_->setChecked(true);
    chkFillNops_->setToolTip("Automatically pad tail bytes with 0x90 (NOP) if new instruction is shorter than original.");
    optionsLayout->addWidget(chkFillNops_);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    lblStatus_ = new QLabel(this);
    lblStatus_->setWordWrap(true);
    lblStatus_->setText("Enter x86_64 assembly instruction and press Enter or Assemble.");
    lblStatus_->setStyleSheet("color: #90a4ae;");
    mainLayout->addWidget(lblStatus_);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    btnAssemble_ = new QPushButton("&Assemble", this);
    btnAssemble_->setDefault(true);
    btnLayout->addWidget(btnAssemble_);

    btnClose_ = new QPushButton("&Close", this);
    btnLayout->addWidget(btnClose_);

    mainLayout->addLayout(btnLayout);

    connect(btnAssemble_, &QPushButton::clicked, this, &AssembleDialog::onAssembleClicked);
    connect(txtAsm_, &QLineEdit::returnPressed, this, &AssembleDialog::onAssembleClicked);
    connect(btnClose_, &QPushButton::clicked, this, &QDialog::reject);
}

void AssembleDialog::setTargetInstruction(Address addr, size_t originalLen, const QString& defaultAsm) {
    currentAddr_ = addr;
    origLen_ = originalLen;

    if (lblAddress_) {
        lblAddress_->setText(QString("Target Address: <b style='color:#61afef;'>%1</b> (Original length: %2 bytes)")
                                 .arg(currentAddr_.toQString())
                                 .arg(origLen_));
    }
    if (txtAsm_) {
        txtAsm_->setText(defaultAsm);
        txtAsm_->selectAll();
        txtAsm_->setFocus();
    }
}

bool AssembleDialog::fillWithNops() const noexcept {
    return chkFillNops_ && chkFillNops_->isChecked();
}

void AssembleDialog::onAssembleClicked() {
    if (!session_) return;

    QString text = txtAsm_->text().trimmed();
    if (text.isEmpty()) return;

    auto res = session_->assemble(text.toStdString(), currentAddr_);
    if (!res) {
        lblStatus_->setStyleSheet("color: #ff5252; font-weight: bold;");
        lblStatus_->setText(QString("Assemble Failed: %1").arg(QString::fromStdString(res.error)));
        txtAsm_->selectAll();
        txtAsm_->setFocus();
        return;
    }

    std::vector<uint8_t> newBytes = std::move(res.value);
    size_t assembledLen = newBytes.size();

    // Auto-pad with NOPs if enabled and shorter than replaced instruction
    if (chkFillNops_->isChecked() && origLen_ > 0 && assembledLen < origLen_) {
        size_t padCount = origLen_ - assembledLen;
        newBytes.insert(newBytes.end(), padCount, 0x90);
    }

    if (!session_->writeMemory(currentAddr_, newBytes.data(), newBytes.size())) {
        lblStatus_->setStyleSheet("color: #ff5252; font-weight: bold;");
        lblStatus_->setText(QString("Write Error: Failed to write %1 bytes to memory at %2")
                                .arg(newBytes.size())
                                .arg(currentAddr_.toQString()));
        return;
    }

    Address assembledAddr = currentAddr_;
    size_t bytesWritten = newBytes.size();
    Address nextAddr = assembledAddr + bytesWritten;

    lblStatus_->setStyleSheet("color: #50dca0; font-weight: bold;");
    lblStatus_->setText(QString("✓ Assembled %1 byte(s) at %2. Advanced to %3.")
                            .arg(bytesWritten)
                            .arg(assembledAddr.toQString())
                            .arg(nextAddr.toQString()));

    // Emit notification so parent view can refresh and advance selection
    Q_EMIT instructionAssembled(assembledAddr, bytesWritten, nextAddr);
}

} // namespace edb_next
