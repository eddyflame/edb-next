#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QTableWidget>
#include <QPushButton>

namespace edb_next {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override = default;

    void addPluginOptionsPage(QWidget* page, const QString& title);

private Q_SLOTS:
    void onApplyClicked();
    void onOkClicked();
    void onResetSignalsClicked();
    void onBrowseSessionDir();
    void onBrowsePluginDir();
    void onBrowseSymbolDir();
    void onBrowseScriptDir();
    void onDisasmFontChoose();
    void onRegFontChoose();
    void onStackFontChoose();
    void onHexFontChoose();

private:
    void setupUi();
    void loadFromConfig();
    void saveToConfig();

    QTabWidget* tabWidget_{nullptr};

    // General Tab
    QRadioButton* rdoClosePrompt_{nullptr};
    QRadioButton* rdoCloseDetach_{nullptr};
    QRadioButton* rdoCloseTerminate_{nullptr};
    QCheckBox* chkRestoreGeometry_{nullptr};
    QLineEdit* txtSessionDir_{nullptr};

    // Appearance Tab
    QComboBox* cmbTheme_{nullptr};
    QPushButton* btnDisasmFont_{nullptr};
    QPushButton* btnRegFont_{nullptr};
    QPushButton* btnStackFont_{nullptr};
    QPushButton* btnHexFont_{nullptr};
    QFont fontDisasm_;
    QFont fontReg_;
    QFont fontStack_;
    QFont fontHex_;
    QCheckBox* chkAddressColon_{nullptr};
    QCheckBox* chkJumpArrows_{nullptr};
    QCheckBox* chkHighlightRegs_{nullptr};

    // Debug Engine Tab
    QCheckBox* chkDisableASLR_{nullptr};
    QCheckBox* chkDisableLazyBinding_{nullptr};
    QRadioButton* rdoBpEntry_{nullptr};
    QRadioButton* rdoBpMain_{nullptr};
    QRadioButton* rdoBpNone_{nullptr};
    QCheckBox* chkBreakOnLibLoad_{nullptr};
    QCheckBox* chkPtyTerminal_{nullptr};
    QLineEdit* txtPtyCommand_{nullptr};
    QComboBox* cmbDefaultBpType_{nullptr};

    // Disassembly Tab
    QRadioButton* rdoSyntaxIntel_{nullptr};
    QRadioButton* rdoSyntaxATT_{nullptr};
    QCheckBox* chkUppercase_{nullptr};
    QCheckBox* chkSymbolicAddrs_{nullptr};
    QCheckBox* chkSimplifyRip_{nullptr};
    QCheckBox* chkShowBytesHex_{nullptr};
    QSpinBox* spnTabSize_{nullptr};

    // Signals Tab
    QTableWidget* tableSignals_{nullptr};
    QPushButton* btnResetSignals_{nullptr};

    // Directories Tab
    QLineEdit* txtPluginDir_{nullptr};
    QLineEdit* txtSymbolDir_{nullptr};
    QLineEdit* txtScriptDir_{nullptr};

    // Plugins Tab
    QTabWidget* pluginTabs_{nullptr};
};

} // namespace edb_next
