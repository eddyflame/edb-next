#include "ui/MainWindow.hpp"
#include "core/ConfigurationManager.hpp"
#include <QApplication>
#include <QTimer>
#include <iostream>
#include <unistd.h>

std::string getTargetBinary() {
    if (access("./test_target", F_OK) == 0) return "./test_target";
    if (access("./build/test_target", F_OK) == 0) return "./build/test_target";
    return "../build/test_target";
}

int main(int argc, char* argv[]) {
    std::cout << "[TEST] Starting Enhanced Exit test (Active Process Teardown)..." << std::endl;
    edb_next::ConfigurationManager::instance().general().closeBehavior = edb_next::CloseBehavior::Terminate;
    QApplication app(argc, argv);

    auto* w = new edb_next::MainWindow();
    w->show();

    // Launch a process in the active session
    auto session = w->sessionManager().activeSession();
    if (session) {
        std::string target = getTargetBinary();
        bool ok = session->launch(target, {"WorkerExitTest"});
        std::cout << "[INFO] Launched target with PID: " << session->pid() << ", launch success: " << ok << std::endl;
        session->resume(); // Target is actively running!
    }

    // Trigger close after 150ms while process is actively running
    QTimer::singleShot(150, [w, &app]() {
        std::cout << "[INFO] Triggering window close() with active running target..." << std::endl;
        w->close();
        delete w;
        std::cout << "[INFO] MainWindow deleted successfully while target was running." << std::endl;
        app.quit();
    });

    int ret = app.exec();
    std::cout << "[PASS] Application exited cleanly with code: " << ret << std::endl;
    return ret;
}
