#include "NotesView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontDatabase>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <QFile>

namespace edb_next {

NotesView::NotesView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void NotesView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Toolbar
    auto* tb = new QHBoxLayout();
    tb->setContentsMargins(2, 2, 2, 2);

    btnInsertRip_ = new QPushButton("Insert Current RIP", this);
    connect(btnInsertRip_, &QPushButton::clicked, this, &NotesView::insertCurrentRip);

    btnInsertTime_ = new QPushButton("Insert Timestamp", this);
    connect(btnInsertTime_, &QPushButton::clicked, this, &NotesView::insertTimestamp);

    btnSaveDb_ = new QPushButton("Save to Database", this);
    btnSaveDb_->setStyleSheet("background-color: #2b5b84; color: white; font-weight: bold;");
    connect(btnSaveDb_, &QPushButton::clicked, this, &NotesView::requestSaveDatabase);

    btnExport_ = new QPushButton("Export Notes...", this);
    connect(btnExport_, &QPushButton::clicked, this, &NotesView::exportNotesToFile);

    btnClear_ = new QPushButton("Clear", this);
    connect(btnClear_, &QPushButton::clicked, this, &NotesView::clearNotes);

    tb->addWidget(btnInsertRip_);
    tb->addWidget(btnInsertTime_);
    tb->addStretch(1);
    tb->addWidget(btnSaveDb_);
    tb->addWidget(btnExport_);
    tb->addWidget(btnClear_);

    layout->addLayout(tb);

    // Plain text editor
    editor_ = new QPlainTextEdit(this);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);
    editor_->setFont(mono);
    editor_->setPlaceholderText("Record reverse engineering notes, struct definitions, shellcode, keys, and analysis here...\n(Automatically saved to project database)");

    connect(editor_, &QPlainTextEdit::textChanged, this, &NotesView::notesModified);

    layout->addWidget(editor_);
}

void NotesView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
}

QString NotesView::notesText() const {
    return editor_->toPlainText();
}

void NotesView::setNotesText(const QString& text) {
    editor_->setPlainText(text);
}

void NotesView::insertCurrentRip() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        editor_->insertPlainText("[RIP: N/A - Target not running]\n");
        return;
    }

    Address rip = session->registers().rip();
    std::string sym_str;
    if (auto sym = session->symbols().findNearestSymbol(rip)) {
        sym_str = " <" + sym->first.name + ">";
    }

    QString tag = QString("[Address: %1%2]\n")
        .arg(QString::fromStdString(rip.toHex()))
        .arg(QString::fromStdString(sym_str));
    editor_->insertPlainText(tag);
}

void NotesView::insertTimestamp() {
    QString timeStr = QString("[Time: %1]\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    editor_->insertPlainText(timeStr);
}

void NotesView::exportNotesToFile() {
    QString path = QFileDialog::getSaveFileName(this, "Export Notes", "notes.md", "Markdown / Text (*.md *.txt)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Failed to open file: " + path);
        return;
    }

    QTextStream out(&file);
    out << editor_->toPlainText();
    file.close();

    QMessageBox::information(this, "Export Notes", "Notes exported successfully to: " + path);
}

void NotesView::clearNotes() {
    if (editor_->toPlainText().isEmpty()) return;
    auto res = QMessageBox::question(this, "Clear Notes", "Are you sure you want to clear all notes?", QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        editor_->clear();
    }
}

} // namespace edb_next
