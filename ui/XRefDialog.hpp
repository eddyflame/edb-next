#pragma once

#include "CodeXRefFinder.hpp"
#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

namespace edb_next {

class XRefDialog : public QDialog {
    Q_OBJECT

public:
    explicit XRefDialog(Address targetAddr, const std::vector<CodeXRef>& xrefs, QWidget* parent = nullptr);
    ~XRefDialog() override = default;

Q_SIGNALS:
    void jumpRequested(Address addr);

private Q_SLOTS:
    void onCellDoubleClicked(int row, int col);

private:
    Address targetAddr_;
    std::vector<CodeXRef> xrefs_;
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
