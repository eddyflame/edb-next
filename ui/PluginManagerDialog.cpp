#include "PluginManagerDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>

namespace edb_next {

PluginManagerDialog::PluginManagerDialog(PluginManager& pluginMgr, QWidget* parent)
    : QDialog(parent), pluginMgr_(pluginMgr)
{
    setupUi();
    onRefreshTable();
}

void PluginManagerDialog::setupUi() {
    setWindowTitle("Plugin Manager - edb-next");
    resize(640, 420);

    auto* root_layout = new QVBoxLayout(this);

    auto* top_layout = new QHBoxLayout();
    btnLoadFile_ = new QPushButton("Load Plugin (.so)...", this);
    connect(btnLoadFile_, &QPushButton::clicked, this, &PluginManagerDialog::onLoadPluginFileClicked);
    btnRefresh_ = new QPushButton("Refresh List", this);
    connect(btnRefresh_, &QPushButton::clicked, this, &PluginManagerDialog::onRefreshTable);
    top_layout->addWidget(new QLabel("Installed Plugins:"));
    top_layout->addStretch();
    top_layout->addWidget(btnLoadFile_);
    top_layout->addWidget(btnRefresh_);
    root_layout->addLayout(top_layout);

    tablePlugins_ = new QTableWidget(this);
    tablePlugins_->setColumnCount(5);
    tablePlugins_->setHorizontalHeaderLabels({"Enabled", "Name", "Version", "Author", "ID"});
    tablePlugins_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tablePlugins_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tablePlugins_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tablePlugins_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tablePlugins_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tablePlugins_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tablePlugins_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(tablePlugins_, &QTableWidget::itemSelectionChanged, this, &PluginManagerDialog::onSelectionChanged);
    connect(tablePlugins_, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (column == 0) {
            auto* item = tablePlugins_->item(row, column);
            auto* id_item = tablePlugins_->item(row, 4);
            if (item && id_item) {
                bool enabled = (item->checkState() == Qt::Checked);
                pluginMgr_.enablePlugin(id_item->text().toStdString(), enabled);
            }
        }
    });
    root_layout->addWidget(tablePlugins_);

    root_layout->addWidget(new QLabel("Plugin Details & Description:"));
    txtDescription_ = new QPlainTextEdit(this);
    txtDescription_->setReadOnly(true);
    txtDescription_->setMaximumHeight(90);
    root_layout->addWidget(txtDescription_);

    auto* btn_close = new QPushButton("Close", this);
    connect(btn_close, &QPushButton::clicked, this, &QDialog::accept);
    auto* bottom_layout = new QHBoxLayout();
    bottom_layout->addStretch();
    bottom_layout->addWidget(btn_close);
    root_layout->addLayout(bottom_layout);
}

void PluginManagerDialog::onRefreshTable() {
    tablePlugins_->blockSignals(true);
    tablePlugins_->setRowCount(0);

    const auto& list = pluginMgr_.loadedPlugins();
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& p = list[i];
        int row = static_cast<int>(i);
        tablePlugins_->insertRow(row);

        auto* chk_item = new QTableWidgetItem();
        chk_item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        chk_item->setCheckState(p.isEnabled ? Qt::Checked : Qt::Unchecked);
        tablePlugins_->setItem(row, 0, chk_item);

        auto* name_item = new QTableWidgetItem(QString::fromStdString(p.metadata.name));
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        tablePlugins_->setItem(row, 1, name_item);

        auto* ver_item = new QTableWidgetItem(QString::fromStdString(p.metadata.version));
        ver_item->setFlags(ver_item->flags() & ~Qt::ItemIsEditable);
        tablePlugins_->setItem(row, 2, ver_item);

        auto* author_item = new QTableWidgetItem(QString::fromStdString(p.metadata.author));
        author_item->setFlags(author_item->flags() & ~Qt::ItemIsEditable);
        tablePlugins_->setItem(row, 3, author_item);

        auto* id_item = new QTableWidgetItem(QString::fromStdString(p.metadata.id));
        id_item->setFlags(id_item->flags() & ~Qt::ItemIsEditable);
        tablePlugins_->setItem(row, 4, id_item);
    }
    tablePlugins_->blockSignals(false);
    onSelectionChanged();
}

void PluginManagerDialog::onSelectionChanged() {
    int row = tablePlugins_->currentRow();
    if (row < 0 || row >= static_cast<int>(pluginMgr_.loadedPlugins().size())) {
        txtDescription_->setPlainText("Select a plugin above to view details.");
        return;
    }

    const auto& p = pluginMgr_.loadedPlugins()[row];
    QString info = QString("ID: %1\nName: %2\nVersion: %3\nAuthor: %4\nPath: %5\nDescription: %6")
        .arg(QString::fromStdString(p.metadata.id))
        .arg(QString::fromStdString(p.metadata.name))
        .arg(QString::fromStdString(p.metadata.version))
        .arg(QString::fromStdString(p.metadata.author))
        .arg(QString::fromStdString(p.filePath))
        .arg(QString::fromStdString(p.metadata.description));
    txtDescription_->setPlainText(info);
}

void PluginManagerDialog::onLoadPluginFileClicked() {
    QString file = QFileDialog::getOpenFileName(this, "Select Plugin Binary", QString(), "Shared Library (*.so);;All Files (*)");
    if (!file.isEmpty()) {
        bool ok = pluginMgr_.loadPlugin(file);
        if (!ok) {
            QMessageBox::warning(this, "Load Error", "Failed to load specified plugin. Check debugger console for error details.");
        }
        onRefreshTable();
    }
}

} // namespace edb_next
