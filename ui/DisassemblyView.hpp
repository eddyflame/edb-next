#pragma once

#include "Types.hpp"
#include "DebugSession.hpp"
#include <QTableWidget>
#include <memory>
#include <optional>

namespace edb_next {

class DisassemblyView : public QTableWidget {
    Q_OBJECT

public:
    explicit DisassemblyView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();
    [[nodiscard]] const DisassembledInstruction* instructionAtRow(int row) const;

public Q_SLOTS:
    void gotoAddressPrompt();
    void gotoAddress(Address addr);
    void followRip();
    void runToSelection();
    void patchBytesPrompt();
    void fillWithNops();
    void assemblePrompt();
    void findXRefsPrompt();
    void editCommentPrompt();
    void toggleBookmark();
    void gotoNextBookmark();
    void gotoPrevBookmark();
    void setOriginToSelection();
    void followSelectedBranch();
    void navigateHistoryBack();
    void navigateHistoryForward();
    void toggleMixedSourceMode();
    void setMixedSourceMode(bool enabled);
    [[nodiscard]] bool isMixedSourceMode() const noexcept { return showMixedSource_; }

Q_SIGNALS:
    void breakpointToggled(Address addr);
    void instructionInspected(const QString& summary);
    void jumpToMemoryRequested(Address addr);

private Q_SLOTS:
    void handleCellDoubleClicked(int row, int col);
    void handleCustomContextMenu(const QPoint& pos);
    void onCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);

private:
    enum class RowType {
        Instruction,
        SourceBanner
    };

    struct DisplayRow {
        RowType type{RowType::Instruction};
        size_t insnIndex{0};
    };

    void setupUi();
    [[nodiscard]] std::optional<Address> addressAtRow(int row) const;

    std::weak_ptr<DebugSession> session_;
    std::vector<DisassembledInstruction> currentInstructions_;
    std::vector<DisplayRow> displayRows_;
    bool showMixedSource_{true};
    Address viewAddress_{0};
    bool followRip_{true};
    std::vector<Address> navHistory_;
    std::vector<Address> navForward_;
};

} // namespace edb_next
