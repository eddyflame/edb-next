#include "ProcessPropertiesView.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

namespace edb_next {

ProcessPropertiesView::ProcessPropertiesView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ProcessPropertiesView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->setTabPosition(QTabWidget::North);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);

    // Tab 1: Info
    auto* infoWidget = new QWidget(tabWidget_);
    auto* infoLayout = new QVBoxLayout(infoWidget);
    infoLayout->setContentsMargins(2, 2, 2, 2);

    infoTable_ = new QTableWidget(infoWidget);
    infoTable_->setColumnCount(2);
    infoTable_->setHorizontalHeaderLabels({"Property", "Value"});
    infoTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    infoTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    infoTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    infoTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    infoTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    infoTable_->verticalHeader()->setVisible(false);
    infoTable_->setShowGrid(true);
    infoTable_->setFont(monoFont);
    infoLayout->addWidget(infoTable_);
    tabWidget_->addTab(infoWidget, "Process Info");

    // Tab 2: Environment
    auto* envWidget = new QWidget(tabWidget_);
    auto* envLayout = new QVBoxLayout(envWidget);
    envLayout->setContentsMargins(2, 2, 2, 2);
    envLayout->setSpacing(4);

    auto* envTopLayout = new QHBoxLayout();
    auto* envFilterLabel = new QLabel("Filter:", envWidget);
    envTopLayout->addWidget(envFilterLabel);
    envFilterEdit_ = new QLineEdit(envWidget);
    envFilterEdit_->setPlaceholderText("Search environment variable...");
    envFilterEdit_->setClearButtonEnabled(true);
    envTopLayout->addWidget(envFilterEdit_, 1);
    envLayout->addLayout(envTopLayout);

    envTable_ = new QTableWidget(envWidget);
    envTable_->setColumnCount(2);
    envTable_->setHorizontalHeaderLabels({"Variable", "Value"});
    envTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    envTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    envTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    envTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    envTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    envTable_->verticalHeader()->setVisible(false);
    envTable_->setShowGrid(true);
    envTable_->setFont(monoFont);
    envLayout->addWidget(envTable_, 1);
    tabWidget_->addTab(envWidget, "Environment");

    // Tab 3: File Descriptors
    auto* fdWidget = new QWidget(tabWidget_);
    auto* fdLayout = new QVBoxLayout(fdWidget);
    fdLayout->setContentsMargins(2, 2, 2, 2);
    fdLayout->setSpacing(4);

    auto* fdTopLayout = new QHBoxLayout();
    auto* fdFilterLabel = new QLabel("Filter:", fdWidget);
    fdTopLayout->addWidget(fdFilterLabel);
    fdFilterEdit_ = new QLineEdit(fdWidget);
    fdFilterEdit_->setPlaceholderText("Search file descriptor or target...");
    fdFilterEdit_->setClearButtonEnabled(true);
    fdTopLayout->addWidget(fdFilterEdit_, 1);
    fdLayout->addLayout(fdTopLayout);

    fdTable_ = new QTableWidget(fdWidget);
    fdTable_->setColumnCount(3);
    fdTable_->setHorizontalHeaderLabels({"FD", "Type", "Target Link / Path"});
    fdTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    fdTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    fdTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    fdTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fdTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    fdTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fdTable_->verticalHeader()->setVisible(false);
    fdTable_->setShowGrid(true);
    fdTable_->setFont(monoFont);
    fdLayout->addWidget(fdTable_, 1);
    tabWidget_->addTab(fdWidget, "File Descriptors");

    mainLayout->addWidget(tabWidget_, 1);

    connect(envFilterEdit_, &QLineEdit::textChanged, this, &ProcessPropertiesView::onEnvFilterChanged);
    connect(fdFilterEdit_, &QLineEdit::textChanged, this, &ProcessPropertiesView::onFdFilterChanged);
}

void ProcessPropertiesView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void ProcessPropertiesView::refresh() {
    dirty_ = false;
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped || session->pid() <= 0) {
        infoTable_->setRowCount(0);
        envTable_->setRowCount(0);
        fdTable_->setRowCount(0);
        return;
    }

    Pid pid = session->pid();
    updateInfoTab(pid);
    updateEnvTab(pid);
    updateFdTab(pid);
}

void ProcessPropertiesView::updateInfoTab(Pid pid) {
    infoTable_->setRowCount(0);

    auto addRow = [this](const QString& prop, const QString& val) {
        int row = infoTable_->rowCount();
        infoTable_->insertRow(row);
        auto* itemProp = new QTableWidgetItem(prop);
        QFont boldFont = infoTable_->font();
        boldFont.setBold(true);
        itemProp->setFont(boldFont);
        auto* itemVal = new QTableWidgetItem(val);
        itemVal->setFont(infoTable_->font());
        infoTable_->setItem(row, 0, itemProp);
        infoTable_->setItem(row, 1, itemVal);
    };

    addRow("Process ID (PID)", QString::number(pid));

    // Read cmdline
    std::string cmdlinePath = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream cmdFile(cmdlinePath, std::ios::binary);
    if (cmdFile.is_open()) {
        std::string buffer((std::istreambuf_iterator<char>(cmdFile)), std::istreambuf_iterator<char>());
        QStringList args;
        size_t start = 0;
        for (size_t i = 0; i < buffer.size(); ++i) {
            if (buffer[i] == '\0') {
                args << QString::fromStdString(buffer.substr(start, i - start));
                start = i + 1;
            }
        }
        addRow("Command Line", args.join(" "));
    }

    // Read status
    std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream statusFile(statusPath);
    if (statusFile.is_open()) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.starts_with("State:") || line.starts_with("PPid:") ||
                line.starts_with("Threads:") || line.starts_with("VmSize:") ||
                line.starts_with("VmRSS:") || line.starts_with("VmExe:")) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());
                    addRow(QString::fromStdString(key), QString::fromStdString(val));
                }
            }
        }
    }
}

void ProcessPropertiesView::updateEnvTab(Pid pid) {
    envTable_->setRowCount(0);

    std::string envPath = "/proc/" + std::to_string(pid) + "/environ";
    std::ifstream envFile(envPath, std::ios::binary);
    if (!envFile.is_open()) return;

    std::string buffer((std::istreambuf_iterator<char>(envFile)), std::istreambuf_iterator<char>());
    QString filter = envFilterEdit_->text().trimmed();

    size_t start = 0;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '\0') {
            std::string entry = buffer.substr(start, i - start);
            start = i + 1;

            size_t eq = entry.find('=');
            if (eq != std::string::npos) {
                QString key = QString::fromStdString(entry.substr(0, eq));
                QString val = QString::fromStdString(entry.substr(eq + 1));

                if (!filter.isEmpty() && !key.contains(filter, Qt::CaseInsensitive) && !val.contains(filter, Qt::CaseInsensitive)) {
                    continue;
                }

                int row = envTable_->rowCount();
                envTable_->insertRow(row);
                auto* itemKey = new QTableWidgetItem(key);
                auto* itemVal = new QTableWidgetItem(val);
                itemKey->setFont(envTable_->font());
                itemVal->setFont(envTable_->font());
                itemKey->setForeground(QColor(100, 200, 255));
                envTable_->setItem(row, 0, itemKey);
                envTable_->setItem(row, 1, itemVal);
            }
        }
    }
}

void ProcessPropertiesView::updateFdTab(Pid pid) {
    fdTable_->setRowCount(0);

    std::string fdDirPath = "/proc/" + std::to_string(pid) + "/fd";
    std::error_code ec;
    if (!fs::exists(fdDirPath, ec)) return;

    QString filter = fdFilterEdit_->text().trimmed();

    fs::directory_iterator it(fdDirPath, ec);
    if (ec) return;

    for (const auto& entry : it) {
        QString fdStr = QString::fromStdString(entry.path().filename().string());

        std::error_code sym_ec;
        auto targetPath = fs::read_symlink(entry.path(), sym_ec);
        QString targetStr = sym_ec ? "[unreadable]" : QString::fromStdString(targetPath.string());

        QString typeStr = "File";
        if (targetStr.startsWith("socket:")) typeStr = "Socket";
        else if (targetStr.startsWith("pipe:")) typeStr = "Pipe";
        else if (targetStr.startsWith("anon_inode:")) typeStr = "Anon Inode";
        else if (targetStr.startsWith("/dev/pts/")) typeStr = "Terminal / PTY";

        if (!filter.isEmpty() && !fdStr.contains(filter, Qt::CaseInsensitive) && !targetStr.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        int row = fdTable_->rowCount();
        fdTable_->insertRow(row);

        auto* itemFd = new QTableWidgetItem(fdStr);
        auto* itemType = new QTableWidgetItem(typeStr);
        auto* itemTarget = new QTableWidgetItem(targetStr);

        itemFd->setFont(fdTable_->font());
        itemType->setFont(fdTable_->font());
        itemTarget->setFont(fdTable_->font());

        if (typeStr == "Socket") itemType->setForeground(QColor(230, 180, 80));
        else if (typeStr == "Pipe") itemType->setForeground(QColor(180, 140, 255));
        else if (typeStr == "Terminal / PTY") itemType->setForeground(QColor(100, 220, 150));

        fdTable_->setItem(row, 0, itemFd);
        fdTable_->setItem(row, 1, itemType);
        fdTable_->setItem(row, 2, itemTarget);
    }
}

void ProcessPropertiesView::onEnvFilterChanged(const QString& /*filter*/) {
    auto session = session_.lock();
    if (session && session->pid() > 0) {
        updateEnvTab(session->pid());
    }
}

void ProcessPropertiesView::onFdFilterChanged(const QString& /*filter*/) {
    auto session = session_.lock();
    if (session && session->pid() > 0) {
        updateFdTab(session->pid());
    }
}

} // namespace edb_next
