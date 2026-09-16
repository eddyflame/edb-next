#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStringList>
#include <vector>
#include <string>

namespace edb_next {

class LaunchArgumentsDialog : public QDialog {
    Q_OBJECT

public:
    explicit LaunchArgumentsDialog(const QString& binaryPath,
                                   const std::vector<std::string>& currentArgs,
                                   const QString& currentWorkingDir,
                                   QWidget* parent = nullptr);
    ~LaunchArgumentsDialog() override = default;

    [[nodiscard]] std::vector<std::string> arguments() const;
    [[nodiscard]] QString workingDirectory() const;
    [[nodiscard]] QString binaryPath() const;

private Q_SLOTS:
    void onBrowseBinary();
    void onBrowseWorkingDir();

private:
    void setupUi();

    QLineEdit* txtBinary_{nullptr};
    QPlainTextEdit* txtArgs_{nullptr};
    QLineEdit* txtWorkDir_{nullptr};
};

} // namespace edb_next
