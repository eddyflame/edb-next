#include "SamplePlugin.hpp"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>

namespace edb_next {

bool SamplePlugin::initialize(IPluginContext* context) {
    ctx_ = context;
    if (!ctx_) return false;

    // Register a custom CLI command for the Command Bar!
    ctx_->registerCommand("sample_ping", [this](const std::vector<std::string>& args) {
        QString msg = "PONG! SamplePlugin received command with " + QString::number(args.size()) + " argument(s).";
        ctx_->logMessage(msg);
    }, "sample_ping [args...] - Test command provided by SamplePlugin");

    // Hook into debug event stream
    ctx_->registerDebugEventListener([this](const DebugEvent& ev) {
        if (ev.reason == StopReason::Breakpoint) {
            ctx_->logMessage(QString("[SamplePlugin Hook] Breakpoint hit detected at %1!").arg(QString::fromStdString(ev.address.toHex())));
        }
    });

    return true;
}

void SamplePlugin::shutdown() {
    if (ctx_) {
        ctx_->logMessage("[SamplePlugin] Shutting down plugin cleanly.");
        ctx_ = nullptr;
    }
}

QMenu* SamplePlugin::createMenu(QWidget* parent) {
    auto* menu = new QMenu("Sample Tools", parent);

    auto* act_hello = menu->addAction("Ping Debugger...");
    connect(act_hello, &QAction::triggered, [this]{
        if (ctx_) {
            ctx_->logMessage("[SamplePlugin] Ping action triggered from Plugins menu!");
        }
    });

    auto* act_about = menu->addAction("About Sample Plugin");
    connect(act_about, &QAction::triggered, [parent]{
        QMessageBox::information(parent, "Sample Plugin", "This is an official reference plugin for edb-next C++20 architecture.");
    });

    return menu;
}

std::vector<QAction*> SamplePlugin::contextMenuItems(ContextMenuTarget target, QWidget* parent) {
    std::vector<QAction*> actions;
    if (target == ContextMenuTarget::Disassembly) {
        auto* act = new QAction("Sample: Analyze Instruction", parent);
        connect(act, &QAction::triggered, [this]{
            if (ctx_) ctx_->logMessage("[SamplePlugin] Custom disassembly context menu clicked!");
        });
        actions.push_back(act);
    }
    return actions;
}

QWidget* SamplePlugin::createOptionsPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("<b>Sample Plugin Configuration</b>"));
    layout->addWidget(new QCheckBox("Enable verbose logging of instruction steps", page));
    layout->addWidget(new QCheckBox("Auto-inject ping command on session startup", page));
    layout->addStretch();

    page->setWindowTitle("Activity Logger");
    return page;
}

} // namespace edb_next
