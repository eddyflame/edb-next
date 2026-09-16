#pragma once

#include "core/LogManager.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

namespace edb_next {

class LogView : public QWidget {
    Q_OBJECT

public:
    explicit LogView(QWidget* parent = nullptr);
    ~LogView() override;

public Q_SLOTS:
    void appendLogEntry(const LogEntry& entry);
    void clearLog();
    void exportLogToFile();

private Q_SLOTS:
    void handleFilterChanged();
    void handleCustomContextMenu(const QPoint& pos);

private:
    void setupUi();
    void reloadAllEntries();

    QLineEdit* searchEdit_{nullptr};
    QComboBox* levelFilterCombo_{nullptr};
    QCheckBox* autoScrollCheck_{nullptr};
    QPushButton* clearBtn_{nullptr};
    QPushButton* exportBtn_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
