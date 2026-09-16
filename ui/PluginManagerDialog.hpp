#pragma once

#include "core/PluginManager.hpp"
#include <QDialog>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QPushButton>

namespace edb_next {

class PluginManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit PluginManagerDialog(PluginManager& pluginMgr, QWidget* parent = nullptr);
    ~PluginManagerDialog() override = default;

private Q_SLOTS:
    void onRefreshTable();
    void onLoadPluginFileClicked();
    void onSelectionChanged();

private:
    void setupUi();

    PluginManager& pluginMgr_;
    QTableWidget* tablePlugins_{nullptr};
    QPlainTextEdit* txtDescription_{nullptr};
    QPushButton* btnLoadFile_{nullptr};
    QPushButton* btnRefresh_{nullptr};
};

} // namespace edb_next
