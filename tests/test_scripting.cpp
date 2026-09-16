#include "DebugSession.hpp"
#include "ScriptEngineManager.hpp"
#include "PythonScriptEngine.hpp"
#include "LuaScriptEngine.hpp"
#include "ScriptConsoleView.hpp"
#include "BreakpointManagerView.hpp"
#include <QTableWidget>
#include <QApplication>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace edb_next;

static void test_standalone_python() {
    std::cout << "[TEST] Running test_standalone_python..." << std::endl;
    PythonScriptEngine pyEngine;
    bool initOk = pyEngine.initialize(nullptr);
    assert(initOk && "Python initialization failed");

    // Test print & arithmetic
    auto res1 = pyEngine.executeString("print('Standalone Python: ' + str(20 + 22))");
    assert(res1.success && "Python execution failed");
    assert(res1.output.find("Standalone Python: 42") != std::string::npos && "Output mismatch");

    // Test math module
    auto res2 = pyEngine.executeString("import math\nprint(f'sqrt(144)={int(math.sqrt(144))}')");
    assert(res2.success && "Python math execution failed");
    assert(res2.output.find("sqrt(144)=12") != std::string::npos && "Output mismatch");

    // Test error capture
    auto res3 = pyEngine.executeString("raise ValueError('Custom test error')");
    assert(!res3.success && "Expected failure on exception");
    assert(res3.error.find("ValueError: Custom test error") != std::string::npos && "Exception traceback missing");

    std::cout << "  -> test_standalone_python PASSED" << std::endl;
}

static void test_standalone_lua() {
    std::cout << "[TEST] Running test_standalone_lua..." << std::endl;
    LuaScriptEngine luaEngine;
    bool initOk = luaEngine.initialize(nullptr);
    assert(initOk && "Lua initialization failed");

    // Test print & arithmetic
    auto res1 = luaEngine.executeString("print('Standalone Lua: ' .. (20 + 22))");
    assert(res1.success && "Lua execution failed");
    assert(res1.output.find("Standalone Lua: 42") != std::string::npos && "Output mismatch");

    // Test multiple print arguments & table
    auto res2 = luaEngine.executeString("local t = { x = 10, y = 20 }; print('x is', t.x, 'y is', t.y)");
    assert(res2.success && "Lua table execution failed");
    assert(res2.output.find("x is\t10\ty is\t20") != std::string::npos && "Output mismatch");

    // Test error capture
    auto res3 = luaEngine.executeString("error('Custom lua error')");
    assert(!res3.success && "Expected failure on lua error");
    assert(res3.error.find("Custom lua error") != std::string::npos && "Lua error message missing");

    std::cout << "  -> test_standalone_lua PASSED" << std::endl;
}

static void test_engine_manager_and_files() {
    std::cout << "[TEST] Running test_engine_manager_and_files..." << std::endl;
    ScriptEngineManager mgr;
    mgr.initialize(nullptr);

    // Test routing by name
    auto pyRes = mgr.execute("python", "print('Via Manager Python')");
    assert(pyRes.success && pyRes.output.find("Via Manager Python") != std::string::npos);

    auto luaRes = mgr.execute("lua", "print('Via Manager Lua')");
    assert(luaRes.success && luaRes.output.find("Via Manager Lua") != std::string::npos);

    // Test file execution
    std::string tmpPyPath = "/tmp/edb_test_script.py";
    {
        std::ofstream ofs(tmpPyPath);
        ofs << "print('Hello from script file py!')\n";
    }
    auto filePyRes = mgr.executeFile(tmpPyPath);
    std::filesystem::remove(tmpPyPath);
    assert(filePyRes.success);
    assert(filePyRes.output.find("Hello from script file py!") != std::string::npos);

    std::string tmpLuaPath = "/tmp/edb_test_script.lua";
    {
        std::ofstream ofs(tmpLuaPath);
        ofs << "print('Hello from script file lua!')\n";
    }
    auto fileLuaRes = mgr.executeFile(tmpLuaPath);
    std::filesystem::remove(tmpLuaPath);
    assert(fileLuaRes.success);
    assert(fileLuaRes.output.find("Hello from script file lua!") != std::string::npos);

    std::cout << "  -> test_engine_manager_and_files PASSED" << std::endl;
}

static void test_session_interaction() {
    std::cout << "[TEST] Running test_session_interaction..." << std::endl;

    auto session = std::make_shared<DebugSession>("sess-script", "ScriptTestSession");
    bool launched = session->launch("./build/test_target", {});
    if (!launched) {
        // Try fallback path
        launched = session->launch("./test_target", {});
    }
    assert(launched && "Failed to launch test_target");

    // 1. Python interaction with live target
    auto pyRes = session->scriptEngines().execute("python",
        "import edb\n"
        "p = edb.pid()\n"
        "st = edb.state()\n"
        "regs = edb.get_regs()\n"
        "rip = edb.get_reg('rip')\n"
        "print(f'PY_TARGET: pid={p}, state={st}, rip={hex(rip)}')\n"
    );
    assert(pyRes.success && "Python target interaction failed");
    assert(pyRes.output.find("PY_TARGET: pid=") != std::string::npos);
    assert(pyRes.output.find("state=Paused") != std::string::npos);

    // Read memory via Python
    uint64_t ripVal = session->registers().rip().value();
    std::string readMemCode = "import edb\n"
                              "b = edb.read_memory(" + std::to_string(ripVal) + ", 4)\n"
                              "print(f'READ_BYTES_LEN: {len(b)}')\n";
    auto memRes = session->scriptEngines().execute("python", readMemCode);
    assert(memRes.success);
    assert(memRes.output.find("READ_BYTES_LEN: 4") != std::string::npos);

    // Test Breakpoints via Python
    std::string bpCode = "import edb\n"
                         "ok1 = edb.set_breakpoint(" + std::to_string(ripVal) + ")\n"
                         "ok2 = edb.remove_breakpoint(" + std::to_string(ripVal) + ")\n"
                         "print(f'BP_TEST: set={ok1}, rem={ok2}')\n";
    auto bpRes = session->scriptEngines().execute("python", bpCode);
    assert(bpRes.success);
    assert(bpRes.output.find("BP_TEST: set=True, rem=True") != std::string::npos);

    // 2. Lua interaction with live target
    auto luaRes = session->scriptEngines().execute("lua",
        "local p = edb.pid()\n"
        "local st = edb.state()\n"
        "local regs = edb.get_regs()\n"
        "local rip = edb.get_reg('rip')\n"
        "print(string.format('LUA_TARGET: pid=%d, state=%s, rip=0x%x', p, st, rip))\n"
    );
    assert(luaRes.success && "Lua target interaction failed");
    assert(luaRes.output.find("LUA_TARGET: pid=") != std::string::npos);
    assert(luaRes.output.find("state=Paused") != std::string::npos);

    // Read memory via Lua
    std::string luaMemCode = "local b = edb.read_memory(" + std::to_string(ripVal) + ", 4)\n"
                             "print('LUA_BYTES_LEN: ' .. string.len(b))\n";
    auto luaMemRes = session->scriptEngines().execute("lua", luaMemCode);
    assert(luaMemRes.success);
    assert(luaMemRes.output.find("LUA_BYTES_LEN: 4") != std::string::npos);

    // Test Breakpoints via Lua
    std::string luaBpCode = "local ok1 = edb.set_breakpoint(" + std::to_string(ripVal) + ")\n"
                            "local ok2 = edb.remove_breakpoint(" + std::to_string(ripVal) + ")\n"
                            "print('LUA_BP_TEST: ' .. tostring(ok1) .. ' ' .. tostring(ok2))\n";
    auto luaBpRes = session->scriptEngines().execute("lua", luaBpCode);
    assert(luaBpRes.success);
    assert(luaBpRes.output.find("LUA_BP_TEST: true true") != std::string::npos);

    // 3. UI Console verification
    ScriptConsoleView console;
    console.setSession(session.get());
    console.executeScript("python", "print('UI Console Python OK')");
    console.executeScript("lua", "print('UI Console Lua OK')");

    session->terminate();
    std::cout << "  -> test_session_interaction PASSED" << std::endl;
}

static void test_script_driven_breakpoints() {
    std::cout << "[TEST] Running test_script_driven_breakpoints..." << std::endl;

    auto session = std::make_shared<DebugSession>("sess-hook", "ScriptHookTestSession");
    bool launched = session->launch("./build/test_target", {});
    if (!launched) {
        launched = session->launch("./test_target", {});
    }
    assert(launched && "Failed to launch test_target");

    Address rip = session->registers().rip();
    assert(!rip.isNull() && "RIP must be valid");

    // 1. Test Python executeHook returning False (silent bypass)
    auto* pyEng = session->scriptEngines().engine("python");
    assert(pyEng != nullptr && "Python engine must exist");
    pyEng->setSession(session.get());
    bool pyPause = pyEng->executeHook(
        "val = edb.get_reg('rdi')\n"
        "edb.set_reg('rax', 7777)\n"
        "return False\n"
    );
    assert(!pyPause && "Python hook returning False must signal silent bypass (no pause)");
    assert(session->registers().rax() == 7777 && "Python hook must successfully set register rax to 7777");

    // 2. Test Python executeHook returning True (pause)
    bool pyPause2 = pyEng->executeHook("return True\n");
    assert(pyPause2 && "Python hook returning True must signal pause");

    // 3. Test Lua executeHook returning false (silent bypass)
    auto* luaEng = session->scriptEngines().engine("lua");
    assert(luaEng != nullptr && "Lua engine must exist");
    luaEng->setSession(session.get());
    bool luaPause = luaEng->executeHook(
        "local val = edb.get_reg('rdi')\n"
        "edb.set_reg('rbx', 8888)\n"
        "return false\n"
    );
    assert(!luaPause && "Lua hook returning false must signal silent bypass (no pause)");
    assert(session->registers().rbx() == 8888 && "Lua hook must successfully set register rbx to 8888");

    // 4. Test Lua executeHook returning true (pause)
    bool luaPause2 = luaEng->executeHook("return true\n");
    assert(luaPause2 && "Lua hook returning true must signal pause");

    // 5. Test setBreakpointScript integration
    bool bpSet = session->addBreakpoint(rip);
    assert(bpSet && "addBreakpoint must succeed");

    bool scriptSet = session->setBreakpointScript(rip, "if edb.get_reg('rax') == 7777:\n    return False\nreturn True\n", "python");
    assert(scriptSet && "setBreakpointScript must succeed");

    const auto* bp = session->breakpointManager().getBreakpoint(rip);
    assert(bp != nullptr && "Breakpoint must exist");
    assert(bp->scriptLanguage == "python" && "Breakpoint scriptLanguage must be python");
    assert(bp->scriptCode.find("7777") != std::string::npos && "Breakpoint scriptCode must match");

    // 6. Test BreakpointManagerView UI rendering of script column
    BreakpointManagerView bpView;
    bpView.setSession(session);
    auto* table = bpView.findChild<QTableWidget*>();
    assert(table != nullptr && "Table widget must exist");
    assert(table->columnCount() == 8 && "Table must have 8 columns including Script Action");
    QTableWidgetItem* scriptItem = table->item(0, 7);
    assert(scriptItem != nullptr && "Script Action table item must exist");
    assert(scriptItem->text().contains("[PYTHON]") && "Script Action text must display language");

    session->terminate();
    std::cout << "  -> test_script_driven_breakpoints PASSED" << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "==========================================" << std::endl;
    std::cout << "  Running edb-next Scripting Engine Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    test_standalone_python();
    test_standalone_lua();
    test_engine_manager_and_files();
    test_session_interaction();
    test_script_driven_breakpoints();

    std::cout << "==========================================" << std::endl;
    std::cout << "  ALL SCRIPTING TESTS PASSED (100%)" << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}
