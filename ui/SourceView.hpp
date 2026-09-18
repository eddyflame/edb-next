#pragma once

#include "Types.hpp"
#include "DebugSession.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <memory>
#include <string>
#include <vector>

namespace edb_next {

class SourceView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit SourceView(QWidget* parent = nullptr);
    ~SourceView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;
    void loadFile(const std::string& filePath);
    void scrollToLine(int line);

public Q_SLOTS:
    void stepOverLine();
    void stepIntoLine();
    void toggleBreakpointAtCurrentLine();
    void runToCursor();

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);
    void breakpointToggled(Address addr);

private Q_SLOTS:
    void onFileComboIndexChanged(int index);
    void onCellDoubleClicked(int row, int col);
    void onCustomContextMenu(const QPoint& pos);
    void onSourceLocationChanged(const edb_next::SourceLocation& loc);
    void onRegistersUpdated();
    void onGotoLineTriggered();

private:
    void setupUi();
    void updateBreakpointsAndRip();
    int currentSelectedLine() const;

    std::weak_ptr<DebugSession> session_;
    std::string currentFilePath_;
    int currentRipLine_{-1};

    QComboBox* fileCombo_{nullptr};
    QCheckBox* followRipCheck_{nullptr};
    QLineEdit* gotoLineInput_{nullptr};
    QPushButton* stepOverBtn_{nullptr};
    QPushButton* stepIntoBtn_{nullptr};
    QTableWidget* codeTable_{nullptr};
};

} // namespace edb_next
