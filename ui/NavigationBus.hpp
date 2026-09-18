#pragma once

#include "Types.hpp"
#include <QObject>
#include <QWidget>

namespace edb_next {

/**
 * @brief Centralized navigation bus coordinating cross-view jump and inspection requests.
 *
 * Decouples views from directly binding to each other or requiring SessionTabWidget
 * to manually cross-wire dozens of point-to-point signals.
 */
class NavigationBus : public QObject {
    Q_OBJECT

public:
    explicit NavigationBus(QObject* parent = nullptr);
    ~NavigationBus() override;

public Q_SLOTS:
    void requestDisassembly(Address addr) { Q_EMIT navigateToDisassembly(addr); }
    void requestDump(Address addr, int tabIndex = -1) { Q_EMIT navigateToDump(addr, tabIndex); }
    void requestStack(Address addr) { Q_EMIT navigateToStack(addr); }
    void requestStruct(Address addr) { Q_EMIT navigateToStruct(addr); }
    void requestStringReferences() { Q_EMIT navigateToStringReferences(); }
    void requestIntermodularCalls() { Q_EMIT navigateToIntermodularCalls(); }
    void requestBottomTab(QWidget* widget) { Q_EMIT navigateToBottomTab(widget); }
    void requestBottomTabIndex(int index) { Q_EMIT navigateToBottomTabIndex(index); }
    void notifyBreakpointChanged() { Q_EMIT breakpointChangedNotification(); }

Q_SIGNALS:
    // Core view navigation
    void navigateToDisassembly(Address addr);
    void navigateToDump(Address addr, int dumpTabIndex = -1);
    void navigateToStack(Address addr);
    void navigateToStruct(Address addr);

    // Analysis workflows
    void navigateToStringReferences();
    void navigateToIntermodularCalls();
    void navigateToBottomTab(QWidget* tabWidget);
    void navigateToBottomTabIndex(int tabIndex);

    // State notifications across views
    void breakpointChangedNotification();
};

} // namespace edb_next
