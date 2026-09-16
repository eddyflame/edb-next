#pragma once

#include "core/DebugSession.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <memory>

namespace edb_next {

class BinaryInfoView : public QWidget {
    Q_OBJECT

public:
    explicit BinaryInfoView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

Q_SIGNALS:
    void jumpToAddressRequested(Address addr, bool isExec);

private Q_SLOTS:
    void handleSectionDoubleClicked(int row, int col);
    void handleSegmentDoubleClicked(int row, int col);
    void handleLibraryDoubleClicked(int row, int col);

private:
    void setupUi();
    void populateHeaderInfo(const ElfParser& parser, const std::string& path);
    void populateSections(const ElfParser& parser);
    void populateSegments(const ElfParser& parser);
    void populateDependencies(const ElfParser& parser);
    void populateLibraries(const DebugSession& session);

    std::weak_ptr<DebugSession> session_;
    QTabWidget* subTabs_{nullptr};

    // Header overview labels
    QLabel* lblPath_{nullptr};
    QLabel* lblType_{nullptr};
    QLabel* lblArch_{nullptr};
    QLabel* lblEntry_{nullptr};
    QLabel* lblPhNum_{nullptr};
    QLabel* lblShNum_{nullptr};

    // Tables
    QTableWidget* sectionsTable_{nullptr};
    QTableWidget* segmentsTable_{nullptr};
    QTableWidget* depsTable_{nullptr};
    QTableWidget* libsTable_{nullptr};
};

} // namespace edb_next
