#include "PluginManager.hpp"
#include <QDir>
#include <QLibrary>
#include <QDebug>
#include <iostream>

namespace edb_next {

PluginManager::PluginManager(IPluginContext* ctx, QObject* parent)
    : QObject(parent), ctx_(ctx)
{
}

PluginManager::~PluginManager() {
    unloadAll();
}

void PluginManager::loadPluginsFromDirectory(const QString& dirPath) {
    if (dirPath.isEmpty()) return;

    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
        return;
    }

    const auto entries = dir.entryInfoList(QDir::Files | QDir::Readable);
    for (const auto& fileInfo : entries) {
        if (QLibrary::isLibrary(fileInfo.fileName())) {
            loadPlugin(fileInfo.absoluteFilePath());
        }
    }
}

bool PluginManager::loadPlugin(const QString& fullPath) {
    // Check if already loaded
    for (const auto& p : plugins_) {
        if (p.filePath == fullPath.toStdString()) {
            return false;
        }
    }

    auto loader = std::make_unique<QPluginLoader>(fullPath);
    loader->setLoadHints(QLibrary::ExportExternalSymbolsHint);

    QObject* plugin_obj = loader->instance();
    if (!plugin_obj) {
        qWarning() << "[PluginManager] Failed to load plugin:" << fullPath << "-" << loader->errorString();
        return false;
    }

    auto* plugin = qobject_cast<IPlugin*>(plugin_obj);
    if (!plugin) {
        qWarning() << "[PluginManager] Object in" << fullPath << "does not implement edb_next::IPlugin interface.";
        loader->unload();
        return false;
    }

    PluginMetadata meta = plugin->metadata();

    // Check duplicate ID
    for (const auto& p : plugins_) {
        if (p.metadata.id == meta.id) {
            qWarning() << "[PluginManager] Plugin with ID" << QString::fromStdString(meta.id) << "already registered.";
            loader->unload();
            return false;
        }
    }

    // Initialize plugin
    if (!plugin->initialize(ctx_)) {
        qWarning() << "[PluginManager] Plugin" << QString::fromStdString(meta.name) << "failed initialize().";
        loader->unload();
        return false;
    }

    LoadedPluginInfo info;
    info.filePath = fullPath.toStdString();
    info.metadata = meta;
    info.instance = plugin;
    info.isEnabled = true;
    info.loader = std::move(loader);

    plugins_.push_back(std::move(info));

    if (ctx_) {
        ctx_->logMessage(QString("Loaded plugin: %1 v%2 by %3")
            .arg(QString::fromStdString(meta.name))
            .arg(QString::fromStdString(meta.version))
            .arg(QString::fromStdString(meta.author)));
    }

    Q_EMIT pluginLoaded(meta);
    return true;
}

void PluginManager::unloadAll() {
    for (auto& info : plugins_) {
        if (info.instance) {
            info.instance->shutdown();
            info.instance = nullptr;
        }
        if (info.loader && info.loader->isLoaded()) {
            info.loader->unload();
        }
        Q_EMIT pluginUnloaded(info.metadata.id);
    }
    plugins_.clear();
}

bool PluginManager::enablePlugin(const std::string& id, bool enable) {
    for (auto& info : plugins_) {
        if (info.metadata.id == id) {
            info.isEnabled = enable;
            return true;
        }
    }
    return false;
}

IPlugin* PluginManager::findPluginById(const std::string& id) const {
    for (const auto& info : plugins_) {
        if (info.metadata.id == id && info.isEnabled) {
            return info.instance;
        }
    }
    return nullptr;
}

} // namespace edb_next
