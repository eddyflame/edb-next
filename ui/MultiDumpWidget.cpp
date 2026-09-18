#include "MultiDumpWidget.hpp"
#include <QVBoxLayout>

namespace edb_next {

MultiDumpWidget::MultiDumpWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void MultiDumpWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->setTabPosition(QTabWidget::South);

    for (int i = 0; i < 4; ++i) {
        auto* hexView = new MemoryHexView(this);
        dumps_[i] = hexView;
        tabWidget_->addTab(hexView, QString("Dump %1").arg(i + 1));

        connect(hexView, &MemoryHexView::jumpToDisassemblyRequested, this, &MultiDumpWidget::jumpToDisassemblyRequested);
        connect(hexView, &MemoryHexView::jumpToStackRequested, this, &MultiDumpWidget::jumpToStackRequested);
        connect(hexView, &MemoryHexView::jumpToDumpRequested, this, [this](Address addr, int tabIndex) {
            jumpToAddress(addr, tabIndex);
        });
        connect(hexView, &MemoryHexView::inspectWithTypeViewerRequested, this, &MultiDumpWidget::inspectWithTypeViewerRequested);
        connect(hexView, &MemoryHexView::patchCreated, this, &MultiDumpWidget::patchCreated);
    }

    layout->addWidget(tabWidget_);
}

void MultiDumpWidget::setSession(std::shared_ptr<DebugSession> session) {
    for (auto* dump : dumps_) {
        if (dump) dump->setSession(session);
    }
}

void MultiDumpWidget::refresh() {
    for (auto* dump : dumps_) {
        if (dump) dump->refresh();
    }
}

void MultiDumpWidget::jumpToAddress(Address addr, int tabIndex) {
    if (tabIndex >= 0 && tabIndex < 4) {
        tabWidget_->setCurrentIndex(tabIndex);
        if (dumps_[tabIndex]) {
            dumps_[tabIndex]->setBaseAddress(addr);
        }
    } else {
        int cur = tabWidget_->currentIndex();
        if (cur >= 0 && cur < 4 && dumps_[cur]) {
            dumps_[cur]->setBaseAddress(addr);
        }
    }
}

MemoryHexView* MultiDumpWidget::activeDump() const {
    int cur = tabWidget_->currentIndex();
    if (cur >= 0 && cur < 4) {
        return dumps_[cur];
    }
    return dumps_[0];
}

MemoryHexView* MultiDumpWidget::dumpAt(int index) const {
    if (index >= 0 && index < 4) {
        return dumps_[index];
    }
    return nullptr;
}

} // namespace edb_next
