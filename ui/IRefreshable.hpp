#pragma once

namespace edb_next {

/**
 * @brief Interface for UI views that can be refreshed and support lazy/deferred updates.
 *
 * Views implementing IRefreshable track a dirty state. When the debugger session
 * pauses or updates, inactive tabs are marked dirty instead of being rendered immediately,
 * preventing broadcast storms and unnecessary ptrace reads across 20+ tabs.
 */
class IRefreshable {
public:
    virtual ~IRefreshable() = default;

    /// Refresh the view contents with latest session state.
    virtual void refresh() = 0;

    /// Mark this view as dirty so it refreshes when it next becomes visible/active.
    virtual void markDirty() noexcept { dirty_ = true; }

    /// Check if the view is dirty and needs refresh upon tab activation.
    [[nodiscard]] virtual bool isDirty() const noexcept { return dirty_; }

    /// Clear the dirty flag.
    virtual void clearDirty() noexcept { dirty_ = false; }

protected:
    bool dirty_{false};
};

} // namespace edb_next
