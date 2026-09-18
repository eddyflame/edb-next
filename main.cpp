#include "ui/MainWindow.hpp"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QLoggingCategory>

void applyDarkTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette dark_palette;
    dark_palette.setColor(QPalette::Window, QColor(30, 32, 36));
    dark_palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
    dark_palette.setColor(QPalette::Base, QColor(22, 24, 27));
    dark_palette.setColor(QPalette::AlternateBase, QColor(35, 37, 42));
    dark_palette.setColor(QPalette::ToolTipBase, Qt::white);
    dark_palette.setColor(QPalette::ToolTipText, Qt::white);
    dark_palette.setColor(QPalette::Text, QColor(230, 230, 230));
    dark_palette.setColor(QPalette::Button, QColor(45, 48, 54));
    dark_palette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    dark_palette.setColor(QPalette::BrightText, Qt::red);
    dark_palette.setColor(QPalette::Link, QColor(42, 130, 218));
    dark_palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    dark_palette.setColor(QPalette::HighlightedText, Qt::white);

    app.setPalette(dark_palette);

    // Modern High-Tech Dark IDE Stylesheet
    const QString qss = R"(
        QMainWindow {
            background-color: #1a1c20;
        }
        QTabWidget::pane {
            border: 1px solid #2d3139;
            background-color: #181a1f;
            top: -1px;
        }
        QTabBar::tab {
            background-color: #21252b;
            color: #9da5b4;
            padding: 5px 12px;
            border: 1px solid #181a1f;
            border-bottom: none;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
            margin-right: 2px;
            font-size: 9pt;
        }
        QTabBar::tab:selected {
            background-color: #181a1f;
            color: #528bff;
            border-top: 2px solid #528bff;
            font-weight: bold;
        }
        QTabBar::tab:hover:!selected {
            background-color: #282c34;
            color: #abb2bf;
        }
        QTableWidget {
            background-color: #181a1f;
            alternate-background-color: #1f2329;
            color: #abb2bf;
            gridline-color: #282c34;
            selection-background-color: #2c313a;
            selection-color: #61afef;
            border: 1px solid #282c34;
        }
        QHeaderView::section {
            background-color: #21252b;
            color: #828997;
            padding: 4px;
            border: 1px solid #181a1f;
            font-weight: bold;
            font-size: 9pt;
        }
        QScrollBar:vertical {
            background-color: #181a1f;
            width: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background-color: #3e4451;
            min-height: 20px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #4b5263;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #181a1f;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background-color: #3e4451;
            min-width: 20px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #4b5263;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QSplitter::handle {
            background-color: #282c34;
        }
        QSplitter::handle:hover {
            background-color: #528bff;
        }
        QMenuBar {
            background-color: #21252b;
            color: #abb2bf;
            border-bottom: 1px solid #181a1f;
        }
        QMenuBar::item:selected {
            background-color: #2c313a;
            color: #ffffff;
        }
        QMenu {
            background-color: #21252b;
            color: #abb2bf;
            border: 1px solid #181a1f;
        }
        QMenu::item:selected {
            background-color: #2c313a;
            color: #61afef;
        }
        QStatusBar {
            background-color: #21252b;
            color: #828997;
            border-top: 1px solid #181a1f;
        }
        QLineEdit {
            background-color: #1e2227;
            color: #abb2bf;
            border: 1px solid #3e4451;
            border-radius: 3px;
            padding: 2px 4px;
        }
        QLineEdit:focus {
            border: 1px solid #528bff;
        }
        QPushButton {
            background-color: #282c34;
            color: #abb2bf;
            border: 1px solid #3e4451;
            border-radius: 3px;
            padding: 3px 8px;
        }
        QPushButton:hover {
            background-color: #3e4451;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #1e2227;
        }
    )";
    app.setStyleSheet(qss);
}

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>

int main(int argc, char* argv[]) {
    // Suppress Wayland non-critical window activation warnings
    QLoggingCategory::setFilterRules("qt.qpa.wayland.warning=false");

    QApplication app(argc, argv);
    app.setApplicationName("edb-next");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("edb-next");

    QCommandLineParser parser;
    parser.setApplicationDescription("EDB Next: Next-Generation Linux x86-64 Binary Debugger");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    std::cout << "[edb-next] Starting EDB Next v1.0.0 (Linux x86-64 Debugger)..." << std::endl;

    applyDarkTheme(app);

    edb_next::MainWindow w;
    w.show();

    std::cout << "[edb-next] Main window displayed successfully." << std::endl;

    return app.exec();
}

