#pragma once

#include "DebugSession.hpp"
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <memory>

namespace edb_next {

class AssembleDialog : public QDialog {
    Q_OBJECT

public:
    explicit AssembleDialog(std::shared_ptr<DebugSession> session,
                            Address targetAddr,
                            size_t originalLength,
                            const QString& defaultAsm,
                            QWidget* parent = nullptr);
    ~AssembleDialog() override = default;

    void setTargetInstruction(Address addr, size_t originalLen, const QString& defaultAsm);

    [[nodiscard]] Address currentAddress() const noexcept { return currentAddr_; }
    [[nodiscard]] bool fillWithNops() const noexcept;

Q_SIGNALS:
    void instructionAssembled(Address assembledAddr, size_t bytesWritten, Address nextAddr);

private Q_SLOTS:
    void onAssembleClicked();

private:
    void setupUi();

    std::shared_ptr<DebugSession> session_;
    Address currentAddr_;
    size_t origLen_{0};

    QLabel* lblAddress_{nullptr};
    QLineEdit* txtAsm_{nullptr};
    QCheckBox* chkFillNops_{nullptr};
    QLabel* lblStatus_{nullptr};
    QPushButton* btnAssemble_{nullptr};
    QPushButton* btnClose_{nullptr};
};

} // namespace edb_next
