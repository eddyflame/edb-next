#pragma once

#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QDockWidget>
#include <vector>

namespace edb_next {

class IUIPluginContext : public IPluginContext {
public:
    ~IUIPluginContext() override = default;
    virtual void addDockWidget(QDockWidget* dock, Qt::DockWidgetArea area = Qt::BottomDockWidgetArea) = 0;
};

class IUIPlugin : public IPlugin {
public:
    ~IUIPlugin() override = default;

    virtual QMenu* createMenu(QWidget* parent = nullptr) { return nullptr; }
    virtual std::vector<QAction*> contextMenuItems(ContextMenuTarget target, QWidget* parent = nullptr) { return {}; }
    virtual QWidget* createOptionsPage(QWidget* parent = nullptr) { return nullptr; }
};

} // namespace edb_next

#define EDB_NEXT_UIPLUGIN_IID "com.edb_next.IUIPlugin/1.0"
Q_DECLARE_INTERFACE(edb_next::IUIPlugin, EDB_NEXT_UIPLUGIN_IID)
