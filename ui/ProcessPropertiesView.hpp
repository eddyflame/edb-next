#pragma once

#include "Types.hpp"
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <memory>

namespace edb_next {

class DebugSession;

class ProcessPropertiesView : public QWidget {
    Q_OBJECT

public:
    explicit ProcessPropertiesView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

private Q_SLOTS:
    void onEnvFilterChanged(const QString& filter);
    void onFdFilterChanged(const QString& filter);

private:
    void setupUi();
    void updateInfoTab(Pid pid);
    void updateEnvTab(Pid pid);
    void updateFdTab(Pid pid);

    std::weak_ptr<DebugSession> session_;

    QTabWidget* tabWidget_{nullptr};

    // Info Tab
    QTableWidget* infoTable_{nullptr};

    // Env Tab
    QLineEdit* envFilterEdit_{nullptr};
    QTableWidget* envTable_{nullptr};

    // FD Tab
    QLineEdit* fdFilterEdit_{nullptr};
    QTableWidget* fdTable_{nullptr};
};

} // namespace edb_next
