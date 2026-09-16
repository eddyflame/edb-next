#include "BinaryInfoView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <QGroupBox>
#include <iomanip>
#include <sstream>

namespace edb_next {

BinaryInfoView::BinaryInfoView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void BinaryInfoView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    subTabs_ = new QTabWidget(this);

    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(9);

    // Tab 1: ELF Header Overview
    auto* overview_page = new QWidget(this);
    auto* ov_layout = new QVBoxLayout(overview_page);

    auto* gb_file = new QGroupBox("ELF File Information", overview_page);
    auto* form_file = new QFormLayout(gb_file);
    lblPath_ = new QLabel("-", gb_file);
    lblType_ = new QLabel("-", gb_file);
    lblArch_ = new QLabel("-", gb_file);
    lblEntry_ = new QLabel("-", gb_file);
    lblEntry_->setFont(mono);
    lblEntry_->setStyleSheet("color: #4da6ff; font-weight: bold;");
    lblPhNum_ = new QLabel("-", gb_file);
    lblShNum_ = new QLabel("-", gb_file);

    form_file->addRow("Binary Path:", lblPath_);
    form_file->addRow("ELF Type:", lblType_);
    form_file->addRow("Architecture / Machine:", lblArch_);
    form_file->addRow("Entry Point:", lblEntry_);
    form_file->addRow("Program Headers (Segments):", lblPhNum_);
    form_file->addRow("Section Headers:", lblShNum_);

    ov_layout->addWidget(gb_file);
    ov_layout->addStretch(1);
    subTabs_->addTab(overview_page, "ELF Header");

    // Tab 2: Sections Table
    sectionsTable_ = new QTableWidget(this);
    sectionsTable_->setColumnCount(7);
    sectionsTable_->setHorizontalHeaderLabels({"Section Name", "Type", "Flags", "Address", "Offset", "Size", "Align"});
    sectionsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    sectionsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    sectionsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    sectionsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    sectionsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    sectionsTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    sectionsTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    sectionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sectionsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sectionsTable_->verticalHeader()->setVisible(false);
    sectionsTable_->setFont(mono);
    connect(sectionsTable_, &QTableWidget::cellDoubleClicked, this, &BinaryInfoView::handleSectionDoubleClicked);
    subTabs_->addTab(sectionsTable_, "Sections");

    // Tab 3: Program Headers (Segments)
    segmentsTable_ = new QTableWidget(this);
    segmentsTable_->setColumnCount(8);
    segmentsTable_->setHorizontalHeaderLabels({"Segment Type", "Flags", "Offset", "Virtual Address", "Physical Address", "File Size", "Mem Size", "Align"});
    segmentsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    segmentsTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    segmentsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    segmentsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    segmentsTable_->verticalHeader()->setVisible(false);
    segmentsTable_->setFont(mono);
    connect(segmentsTable_, &QTableWidget::cellDoubleClicked, this, &BinaryInfoView::handleSegmentDoubleClicked);
    subTabs_->addTab(segmentsTable_, "Program Headers (Segments)");

    // Tab 4: Dynamic Dependencies (DT_NEEDED)
    depsTable_ = new QTableWidget(this);
    depsTable_->setColumnCount(2);
    depsTable_->setHorizontalHeaderLabels({"Index", "Shared Library (DT_NEEDED)"});
    depsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    depsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    depsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    depsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    depsTable_->verticalHeader()->setVisible(false);
    depsTable_->setFont(mono);
    subTabs_->addTab(depsTable_, "Dynamic Dependencies");

    layout->addWidget(subTabs_);
}

void BinaryInfoView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void BinaryInfoView::refresh() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        lblPath_->setText("-");
        lblType_->setText("-");
        lblArch_->setText("-");
        lblEntry_->setText("-");
        lblPhNum_->setText("-");
        lblShNum_->setText("-");
        sectionsTable_->setRowCount(0);
        segmentsTable_->setRowCount(0);
        depsTable_->setRowCount(0);
        return;
    }

    const auto& parser = session->elfParser();
    populateHeaderInfo(parser, session->targetPath());
    populateSections(parser);
    populateSegments(parser);
    populateDependencies(parser);
}

void BinaryInfoView::populateHeaderInfo(const ElfParser& parser, const std::string& path) {
    const auto& hdr = parser.headerInfo();
    lblPath_->setText(QString::fromStdString(path));
    lblType_->setText(QString::fromStdString(hdr.typeString()));
    lblArch_->setText(QString::fromStdString(hdr.machineString()));
    lblEntry_->setText(QString::fromStdString(hdr.entryPoint.toHex()));
    lblPhNum_->setText(QString::number(hdr.programHeaderCount));
    lblShNum_->setText(QString::number(hdr.sectionHeaderCount));
}

void BinaryInfoView::populateSections(const ElfParser& parser) {
    const auto& sections = parser.sections();
    sectionsTable_->setRowCount(static_cast<int>(sections.size()));

    for (int r = 0; r < static_cast<int>(sections.size()); ++r) {
        const auto& s = sections[r];

        auto* item_name = new QTableWidgetItem(QString::fromStdString(s.name));
        auto* item_type = new QTableWidgetItem(QString::fromStdString(s.typeString));
        auto* item_flags = new QTableWidgetItem(QString::fromStdString(s.flagsString()));
        auto* item_addr = new QTableWidgetItem(QString::fromStdString(s.address.toHex()));
        auto* item_off = new QTableWidgetItem(QString("0x%1").arg(s.offset, 0, 16));
        auto* item_sz = new QTableWidgetItem(QString("0x%1 (%2 B)").arg(s.size, 0, 16).arg(s.size));
        auto* item_al = new QTableWidgetItem(QString::number(s.align));

        if (s.isExecutable()) {
            item_name->setForeground(QColor(240, 90, 90));
            item_flags->setForeground(QColor(240, 90, 90));
        } else if (s.isWritable()) {
            item_name->setForeground(QColor(240, 180, 70));
        } else if (s.name.find("text") != std::string::npos || s.name.find("rodata") != std::string::npos) {
            item_name->setForeground(QColor(100, 180, 240));
        }

        sectionsTable_->setItem(r, 0, item_name);
        sectionsTable_->setItem(r, 1, item_type);
        sectionsTable_->setItem(r, 2, item_flags);
        sectionsTable_->setItem(r, 3, item_addr);
        sectionsTable_->setItem(r, 4, item_off);
        sectionsTable_->setItem(r, 5, item_sz);
        sectionsTable_->setItem(r, 6, item_al);
    }
}

void BinaryInfoView::populateSegments(const ElfParser& parser) {
    const auto& segs = parser.programHeaders();
    segmentsTable_->setRowCount(static_cast<int>(segs.size()));

    for (int r = 0; r < static_cast<int>(segs.size()); ++r) {
        const auto& seg = segs[r];

        auto* item_type = new QTableWidgetItem(QString::fromStdString(seg.typeString));
        auto* item_flags = new QTableWidgetItem(QString::fromStdString(seg.flagsString()));
        auto* item_off = new QTableWidgetItem(QString("0x%1").arg(seg.offset, 0, 16));
        auto* item_vaddr = new QTableWidgetItem(QString::fromStdString(seg.vaddr.toHex()));
        auto* item_paddr = new QTableWidgetItem(QString::fromStdString(seg.paddr.toHex()));
        auto* item_filesz = new QTableWidgetItem(QString("0x%1 (%2 B)").arg(seg.filesz, 0, 16).arg(seg.filesz));
        auto* item_memsz = new QTableWidgetItem(QString("0x%1 (%2 B)").arg(seg.memsz, 0, 16).arg(seg.memsz));
        auto* item_align = new QTableWidgetItem(QString::number(seg.align));

        if (seg.flags & 0x1) { // PF_X
            item_flags->setForeground(QColor(240, 90, 90));
        }

        segmentsTable_->setItem(r, 0, item_type);
        segmentsTable_->setItem(r, 1, item_flags);
        segmentsTable_->setItem(r, 2, item_off);
        segmentsTable_->setItem(r, 3, item_vaddr);
        segmentsTable_->setItem(r, 4, item_paddr);
        segmentsTable_->setItem(r, 5, item_filesz);
        segmentsTable_->setItem(r, 6, item_memsz);
        segmentsTable_->setItem(r, 7, item_align);
    }
}

void BinaryInfoView::populateDependencies(const ElfParser& parser) {
    const auto& deps = parser.dynamicDependencies();
    depsTable_->setRowCount(static_cast<int>(deps.size()));

    for (int r = 0; r < static_cast<int>(deps.size()); ++r) {
        auto* item_idx = new QTableWidgetItem(QString::number(r + 1));
        auto* item_name = new QTableWidgetItem(QString::fromStdString(deps[r]));
        item_name->setForeground(QColor(120, 220, 140));

        depsTable_->setItem(r, 0, item_idx);
        depsTable_->setItem(r, 1, item_name);
    }
}

void BinaryInfoView::handleSectionDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    auto session = session_.lock();
    if (!session) return;

    const auto& sections = session->elfParser().sections();
    if (row >= 0 && row < static_cast<int>(sections.size())) {
        const auto& s = sections[row];
        if (!s.address.isNull()) {
            Q_EMIT jumpToAddressRequested(s.address, s.isExecutable());
        }
    }
}

void BinaryInfoView::handleSegmentDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    auto session = session_.lock();
    if (!session) return;

    const auto& segs = session->elfParser().programHeaders();
    if (row >= 0 && row < static_cast<int>(segs.size())) {
        const auto& seg = segs[row];
        if (!seg.vaddr.isNull()) {
            bool is_x = (seg.flags & 0x1) != 0;
            Q_EMIT jumpToAddressRequested(seg.vaddr, is_x);
        }
    }
}

} // namespace edb_next
