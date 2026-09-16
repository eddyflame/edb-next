#pragma once

#include "IPlugin.hpp"
#include "IPluginContext.hpp"
#include <QObject>
#include <QString>
#include <QPluginLoader>
#include <vector>
#include <memory>
#include <string>

namespace edb_next {

struct LoadedPluginInfo {
    std::string filePath;
    PluginMetadata metadata;
    IPlugin* instance{nullptr};
    bool isEnabled{true};
    std::unique_ptr<QPluginLoader> loader;
};

class PluginManager : public QObject {
    Q_OBJECT

public:
    explicit PluginManager(IPluginContext* ctx, QObject* parent = nullptr);
    ~PluginManager() override;

    void loadPluginsFromDirectory(const QString& dirPath);
    bool loadPlugin(const QString& fullPath);
    void unloadAll();

    [[nodiscard]] const std::vector<LoadedPluginInfo>& loadedPlugins() const noexcept { return plugins_; }
    bool enablePlugin(const std::string& id, bool enable);
    [[nodiscard]] IPlugin* findPluginById(const std::string& id) const;

Q_SIGNALS:
    void pluginLoaded(const PluginMetadata& meta);
    void pluginUnloaded(const std::string& id);

private:
    IPluginContext* ctx_{nullptr};
    std::vector<LoadedPluginInfo> plugins_;
};

} // namespace edb_next
