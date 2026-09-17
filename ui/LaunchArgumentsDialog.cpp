#include "LaunchArgumentsDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QRegularExpression>

namespace edb_next {

LaunchArgumentsDialog::LaunchArgumentsDialog(const QString& binaryPath,
                                             const std::vector<std::string>& currentArgs,
                                             const QString& currentWorkingDir,
                                             QWidget* parent)
    : QDialog(parent)
{
    setupUi();

    txtBinary_->setText(binaryPath);
    txtWorkDir_->setText(currentWorkingDir);

    QStringList arg_list;
    for (const auto& a : currentArgs) {
        arg_list << QString::fromStdString(a);
    }
    txtArgs_->setPlainText(arg_list.join("\n"));
}

void LaunchArgumentsDialog::setupUi() {
    setWindowTitle("Target Arguments & Working Directory - edb-next");
    resize(550, 380);

    auto* root_layout = new QVBoxLayout(this);

    auto* grid = new QGridLayout();

    grid->addWidget(new QLabel("Target Executable:"), 0, 0);
    txtBinary_ = new QLineEdit(this);
    auto* btn_bin = new QPushButton("Browse...", this);
    connect(btn_bin, &QPushButton::clicked, this, &LaunchArgumentsDialog::onBrowseBinary);
    grid->addWidget(txtBinary_, 0, 1);
    grid->addWidget(btn_bin, 0, 2);

    grid->addWidget(new QLabel("Working Directory:"), 1, 0);
    txtWorkDir_ = new QLineEdit(this);
    auto* btn_dir = new QPushButton("Browse...", this);
    connect(btn_dir, &QPushButton::clicked, this, &LaunchArgumentsDialog::onBrowseWorkingDir);
    grid->addWidget(txtWorkDir_, 1, 1);
    grid->addWidget(btn_dir, 1, 2);

    root_layout->addLayout(grid);

    root_layout->addWidget(new QLabel("Command Line Arguments (one per line, or space-separated):"));
    txtArgs_ = new QPlainTextEdit(this);
    QFont mono("Monospace", 9);
    txtArgs_->setFont(mono);
    root_layout->addWidget(txtArgs_);

    auto* btn_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root_layout->addWidget(btn_box);
}

void LaunchArgumentsDialog::onBrowseBinary() {
    QString f = QFileDialog::getOpenFileName(this, "Select Executable", txtBinary_->text(), "All Files (*)");
    if (!f.isEmpty()) {
        txtBinary_->setText(f);
        if (txtWorkDir_->text().isEmpty()) {
            txtWorkDir_->setText(QFileInfo(f).absolutePath());
        }
    }
}

void LaunchArgumentsDialog::onBrowseWorkingDir() {
    QString d = QFileDialog::getExistingDirectory(this, "Select Working Directory", txtWorkDir_->text());
    if (!d.isEmpty()) {
        txtWorkDir_->setText(d);
    }
}

QString LaunchArgumentsDialog::binaryPath() const {
    return txtBinary_->text().trimmed();
}

QString LaunchArgumentsDialog::workingDirectory() const {
    return txtWorkDir_->text().trimmed();
}

std::vector<std::string> LaunchArgumentsDialog::arguments() const {
    std::vector<std::string> result;
    QString raw = txtArgs_->toPlainText().trimmed();
    if (raw.isEmpty()) return result;

    QStringList lines = raw.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            result.push_back(trimmed.toStdString());
        }
    }
    return result;
}

} // namespace edb_next
