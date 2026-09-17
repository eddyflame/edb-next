#pragma once

#include "MemoryHexView.hpp"
#include <QWidget>
#include <QTabWidget>
#include <array>
#include <memory>

namespace edb_next {

class MultiDumpWidget : public QWidget {
    Q_OBJECT

public:
    explicit MultiDumpWidget(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

    void jumpToAddress(Address addr, int tabIndex = -1);
    [[nodiscard]] MemoryHexView* activeDump() const;
    [[nodiscard]] MemoryHexView* dumpAt(int index) const;

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);
    void jumpToStackRequested(Address addr);
    void inspectWithTypeViewerRequested(Address addr);
    void patchCreated(Address addr, const std::vector<uint8_t>& oldBytes, const std::vector<uint8_t>& newBytes, const QString& comment);

private:
    void setupUi();

    QTabWidget* tabWidget_{nullptr};
    std::array<MemoryHexView*, 4> dumps_{nullptr, nullptr, nullptr, nullptr};
};

} // namespace edb_next
