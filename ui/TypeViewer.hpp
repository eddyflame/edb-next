#pragma once

#include "core/Types.hpp"
#include "core/TypeManager.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <memory>

namespace edb_next {

class DebugSession;

class TypeViewer : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit TypeViewer(QWidget* parent = nullptr);
    ~TypeViewer() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void setInspectAddress(Address addr);
    void refresh() override;

Q_SIGNALS:
    void jumpToMemoryRequested(edb_next::Address addr);
    void jumpToDisassemblyRequested(edb_next::Address addr);

private Q_SLOTS:
    void onInspectClicked();
    void onDefineStructClicked();
    void onStructSelected(int index);
    void onTableDoubleClicked(int row, int col);
    void onTableContextMenu(const QPoint& pos);
    void editSelectedField();

private:
    void setupUi();
    void updateStructList();
    Address parseAddressInput() const;

    std::shared_ptr<DebugSession> session_;

    QComboBox* structCombo_{nullptr};
    QLineEdit* addressEdit_{nullptr};
    QPushButton* inspectBtn_{nullptr};
    QPushButton* defineBtn_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QLabel* sizeLabel_{nullptr};

    QTableWidget* fieldTable_{nullptr};

    std::optional<EvaluatedStruct> currentEvaluation_;
};

} // namespace edb_next
