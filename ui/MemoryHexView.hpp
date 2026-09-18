#pragma once

#include "Types.hpp"
#include "DebugSession.hpp"
#include <QTableWidget>
#include <memory>

namespace edb_next {

class MemoryHexView : public QTableWidget {
    Q_OBJECT

public:
    explicit MemoryHexView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void setBaseAddress(Address addr);
    [[nodiscard]] Address baseAddress() const noexcept { return baseAddress_; }
    void refresh();

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);
    void jumpToStackRequested(Address addr);
    void jumpToDumpRequested(Address addr, int tabIndex = -1);
    void inspectWithTypeViewerRequested(Address addr);
    void patchCreated(Address addr, const std::vector<uint8_t>& oldBytes, const std::vector<uint8_t>& newBytes, const QString& comment);

public Q_SLOTS:
    void gotoAddress();
    void findPatternPrompt();
    void editBytesPrompt();
    void dumpMemoryRangePrompt();
    void fillNops();
    void fillZeros();
    void navigateBack();
    void navigateForward();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void handleCustomContextMenu(const QPoint& pos);

private:
    void setupUi();
    [[nodiscard]] Address addressAtCell(int row, int col) const;

    std::weak_ptr<DebugSession> session_;
    Address baseAddress_{0};
    size_t rowCount_{32}; // 32 rows * 16 bytes = 512 bytes per page
    std::vector<Address> navHistory_;
    std::vector<Address> navForward_;
};

} // namespace edb_next
