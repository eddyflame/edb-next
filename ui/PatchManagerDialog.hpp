#pragma once

#include "core/PatchManager.hpp"
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <memory>

namespace edb_next {

class DebugSession;

class PatchManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit PatchManagerDialog(PatchManager& patchMgr, std::shared_ptr<DebugSession> session, QWidget* parent = nullptr);
    ~PatchManagerDialog() override = default;

private Q_SLOTS:
    void onRefreshTable();
    void onRevertClicked();
    void onReapplyClicked();
    void onPatchFileClicked();

private:
    void setupUi();

    PatchManager& patchMgr_;
    std::shared_ptr<DebugSession> session_;

    QTableWidget* tablePatches_{nullptr};
    QPushButton* btnRevert_{nullptr};
    QPushButton* btnReapply_{nullptr};
    QPushButton* btnPatchFile_{nullptr};
};

} // namespace edb_next
