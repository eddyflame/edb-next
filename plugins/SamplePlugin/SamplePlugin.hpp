#pragma once

#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"
#include <QObject>
#include <QMenu>
#include <QWidget>

namespace edb_next {

class SamplePlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EDB_NEXT_PLUGIN_IID)
    Q_INTERFACES(edb_next::IPlugin)

public:
    SamplePlugin() = default;
    ~SamplePlugin() override = default;

    [[nodiscard]] PluginMetadata metadata() const override {
        return PluginMetadata{
            .id = "sample_logger",
            .name = "Activity Logger & Tools Plugin",
            .version = "1.0.0",
            .author = "edb-next Team",
            .description = "Demonstrates modern edb-next C++20 plugin architecture with custom menus, CLI commands, and event hooks."
        };
    }

    bool initialize(IPluginContext* context) override;
    void shutdown() override;

    QMenu* createMenu(QWidget* parent = nullptr) override;
    std::vector<QAction*> contextMenuItems(ContextMenuTarget target, QWidget* parent = nullptr) override;
    QWidget* createOptionsPage(QWidget* parent = nullptr) override;

private:
    IPluginContext* ctx_{nullptr};
};

} // namespace edb_next
