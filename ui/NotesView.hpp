#pragma once

#include "core/DebugSession.hpp"
#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <memory>

namespace edb_next {

class NotesView : public QWidget {
    Q_OBJECT

public:
    explicit NotesView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    [[nodiscard]] QString notesText() const;
    void setNotesText(const QString& text);

Q_SIGNALS:
    void notesModified();
    void requestSaveDatabase();

private Q_SLOTS:
    void insertCurrentRip();
    void insertTimestamp();
    void exportNotesToFile();
    void clearNotes();

private:
    void setupUi();

    std::weak_ptr<DebugSession> session_;
    QPlainTextEdit* editor_{nullptr};
    QPushButton* btnInsertRip_{nullptr};
    QPushButton* btnInsertTime_{nullptr};
    QPushButton* btnSaveDb_{nullptr};
    QPushButton* btnExport_{nullptr};
    QPushButton* btnClear_{nullptr};
};

} // namespace edb_next
