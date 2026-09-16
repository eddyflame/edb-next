#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "PythonScriptEngine.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include "LogManager.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

namespace edb_next {

static PythonScriptEngine* s_currentEngine = nullptr;

namespace {

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::optional<uint64_t> getRegVal(const std::string& regName, const RegisterContext& regs) {
    std::string name = toLower(trim(regName));
    if (name.empty()) return std::nullopt;
    if (name[0] == '$') name = name.substr(1);

    if (name == "rax") return regs.rax();
    if (name == "rbx") return regs.rbx();
    if (name == "rcx") return regs.rcx();
    if (name == "rdx") return regs.rdx();
    if (name == "rsi") return regs.rsi();
    if (name == "rdi") return regs.rdi();
    if (name == "rbp") return regs.rbp().value();
    if (name == "rsp") return regs.rsp().value();
    if (name == "r8") return regs.r8();
    if (name == "r9") return regs.r9();
    if (name == "r10") return regs.r10();
    if (name == "r11") return regs.r11();
    if (name == "r12") return regs.r12();
    if (name == "r13") return regs.r13();
    if (name == "r14") return regs.r14();
    if (name == "r15") return regs.r15();
    if (name == "rip") return regs.rip().value();
    if (name == "rflags" || name == "eflags") return regs.eflags();

    return std::nullopt;
}

bool setRegVal(const std::string& regName, uint64_t val, RegisterContext& regs) {
    std::string name = toLower(trim(regName));
    if (name.empty()) return false;
    if (name[0] == '$') name = name.substr(1);

    if (name == "rax") { regs.setRax(val); return true; }
    if (name == "rbx") { regs.setRbx(val); return true; }
    if (name == "rcx") { regs.setRcx(val); return true; }
    if (name == "rdx") { regs.setRdx(val); return true; }
    if (name == "rsi") { regs.setRsi(val); return true; }
    if (name == "rdi") { regs.setRdi(val); return true; }
    if (name == "rbp") { regs.setRbp(Address(val)); return true; }
    if (name == "rsp") { regs.setRsp(Address(val)); return true; }
    if (name == "r8")  { regs.setR8(val); return true; }
    if (name == "r9")  { regs.setR9(val); return true; }
    if (name == "r10") { regs.setR10(val); return true; }
    if (name == "r11") { regs.setR11(val); return true; }
    if (name == "r12") { regs.setR12(val); return true; }
    if (name == "r13") { regs.setR13(val); return true; }
    if (name == "r14") { regs.setR14(val); return true; }
    if (name == "r15") { regs.setR15(val); return true; }
    if (name == "rip") { regs.setRip(Address(val)); return true; }

    return false;
}

// Python C-API wrappers for edb module
PyObject* py_read_memory(PyObject* /*self*/, PyObject* args) {
    uint64_t addr = 0;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "Kn", &addr, &size)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session || size <= 0) {
        return PyBytes_FromStringAndSize("", 0);
    }

    auto bytes = session->readMemory(Address(addr), static_cast<size_t>(size));
    return PyBytes_FromStringAndSize(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

PyObject* py_write_memory(PyObject* /*self*/, PyObject* args) {
    uint64_t addr = 0;
    const char* data = nullptr;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "Ky#", &addr, &data, &size)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session || size <= 0 || !data) {
        Py_RETURN_FALSE;
    }

    bool ok = session->writeMemory(Address(addr), data, static_cast<size_t>(size));
    if (ok) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyObject* py_get_reg(PyObject* /*self*/, PyObject* args) {
    const char* name = nullptr;
    if (!PyArg_ParseTuple(args, "s", &name)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        Py_RETURN_NONE;
    }

    auto val = getRegVal(name, session->registers());
    if (val) {
        return PyLong_FromUnsignedLongLong(*val);
    }
    Py_RETURN_NONE;
}

PyObject* py_set_reg(PyObject* /*self*/, PyObject* args) {
    const char* name = nullptr;
    uint64_t val = 0;
    if (!PyArg_ParseTuple(args, "sK", &name, &val)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        Py_RETURN_FALSE;
    }

    RegisterContext regs = session->registers();
    if (setRegVal(name, val, regs)) {
        bool ok = session->setRegisters(regs);
        if (ok) {
            Py_RETURN_TRUE;
        }
    }
    Py_RETURN_FALSE;
}

PyObject* py_get_regs(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    PyObject* dict = PyDict_New();
    if (!session) {
        return dict;
    }

    const auto& regs = session->registers();
    auto list = regs.toList();
    for (const auto& item : list) {
        std::string lowerName = toLower(item.name);
        PyObject* val = PyLong_FromUnsignedLongLong(item.value);
        PyDict_SetItemString(dict, lowerName.c_str(), val);
        Py_DECREF(val);
    }
    return dict;
}

PyObject* py_set_breakpoint(PyObject* /*self*/, PyObject* args) {
    uint64_t addr = 0;
    const char* symbol = "";
    if (!PyArg_ParseTuple(args, "K|s", &addr, &symbol)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        Py_RETURN_FALSE;
    }

    bool ok = session->addBreakpoint(Address(addr), symbol ? symbol : "");
    if (ok) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyObject* py_remove_breakpoint(PyObject* /*self*/, PyObject* args) {
    uint64_t addr = 0;
    if (!PyArg_ParseTuple(args, "K", &addr)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        Py_RETURN_FALSE;
    }

    bool ok = session->removeBreakpoint(Address(addr));
    if (ok) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyObject* py_step_into(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        session->stepInto();
    }
    Py_RETURN_NONE;
}

PyObject* py_step_over(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        session->stepOver();
    }
    Py_RETURN_NONE;
}

PyObject* py_step_source(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        session->stepSourceOver();
    }
    Py_RETURN_NONE;
}

PyObject* py_resume(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        session->resume();
    }
    Py_RETURN_NONE;
}

PyObject* py_pause(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        session->pause();
    }
    Py_RETURN_NONE;
}

PyObject* py_resolve_symbol(PyObject* /*self*/, PyObject* args) {
    const char* name = nullptr;
    if (!PyArg_ParseTuple(args, "s", &name)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        Py_RETURN_NONE;
    }

    auto addr = session->resolveSymbol(name);
    if (addr) {
        return PyLong_FromUnsignedLongLong(addr->value());
    }
    Py_RETURN_NONE;
}

PyObject* py_eval(PyObject* /*self*/, PyObject* args) {
    const char* expr = nullptr;
    if (!PyArg_ParseTuple(args, "s", &expr)) {
        return nullptr;
    }

    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        Py_RETURN_NONE;
    }

    auto val = ExpressionEvaluator::evaluate(expr, session->registers());
    if (val) {
        return PyLong_FromUnsignedLongLong(*val);
    }
    Py_RETURN_NONE;
}

PyObject* py_pid(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        return PyLong_FromLong(session->pid());
    }
    return PyLong_FromLong(0);
}

PyObject* py_tid(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (session) {
        return PyLong_FromLong(session->activeTid());
    }
    return PyLong_FromLong(0);
}

PyObject* py_state(PyObject* /*self*/, PyObject* /*args*/) {
    auto* session = PythonScriptEngine::activeSession();
    if (!session) {
        return PyUnicode_FromString("None");
    }
    switch (session->state()) {
        case SessionState::Running: return PyUnicode_FromString("Running");
        case SessionState::Paused: return PyUnicode_FromString("Paused");
        case SessionState::Stopped: return PyUnicode_FromString("Stopped");
        case SessionState::Terminated: return PyUnicode_FromString("Terminated");
    }
    return PyUnicode_FromString("Unknown");
}

PyObject* py_log(PyObject* /*self*/, PyObject* args) {
    const char* msg = nullptr;
    if (!PyArg_ParseTuple(args, "s", &msg)) {
        return nullptr;
    }
    LogManager::instance().log(LogLevel::Info, "Python", msg ? msg : "");
    Py_RETURN_NONE;
}

static PyMethodDef EdbMethods[] = {
    {"read_memory", py_read_memory, METH_VARARGS, "Read memory: read_memory(addr, size) -> bytes"},
    {"write_memory", py_write_memory, METH_VARARGS, "Write memory: write_memory(addr, bytes) -> bool"},
    {"get_reg", py_get_reg, METH_VARARGS, "Get register value: get_reg(name) -> int or None"},
    {"set_reg", py_set_reg, METH_VARARGS, "Set register value: set_reg(name, val) -> bool"},
    {"get_regs", py_get_regs, METH_NOARGS, "Get all registers: get_regs() -> dict"},
    {"set_breakpoint", py_set_breakpoint, METH_VARARGS, "Set breakpoint: set_breakpoint(addr, symbol='') -> bool"},
    {"remove_breakpoint", py_remove_breakpoint, METH_VARARGS, "Remove breakpoint: remove_breakpoint(addr) -> bool"},
    {"step_into", py_step_into, METH_NOARGS, "Step into instruction"},
    {"step_over", py_step_over, METH_NOARGS, "Step over instruction"},
    {"step_source", py_step_source, METH_NOARGS, "Step source line"},
    {"resume", py_resume, METH_NOARGS, "Resume execution"},
    {"pause", py_pause, METH_NOARGS, "Pause execution"},
    {"resolve_symbol", py_resolve_symbol, METH_VARARGS, "Resolve symbol name: resolve_symbol(name) -> int or None"},
    {"eval", py_eval, METH_VARARGS, "Evaluate expression: eval(expr) -> int or None"},
    {"pid", py_pid, METH_NOARGS, "Get target process PID: pid() -> int"},
    {"tid", py_tid, METH_NOARGS, "Get active thread TID: tid() -> int"},
    {"state", py_state, METH_NOARGS, "Get session state: state() -> str"},
    {"log", py_log, METH_VARARGS, "Log message: log(msg)"},
    {nullptr, nullptr, 0, nullptr}
};

static struct PyModuleDef edbmodule = {
    PyModuleDef_HEAD_INIT,
    "edb",
    "edb-next embedded scripting API",
    -1,
    EdbMethods,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

PyMODINIT_FUNC PyInit_edb(void) {
    return PyModule_Create(&edbmodule);
}

} // namespace

PythonScriptEngine::PythonScriptEngine() = default;

PythonScriptEngine::~PythonScriptEngine() {
    shutdown();
}

DebugSession* PythonScriptEngine::activeSession() noexcept {
    if (s_currentEngine) {
        return s_currentEngine->session_;
    }
    return nullptr;
}

void PythonScriptEngine::setSession(DebugSession* session) {
    session_ = session;
}

bool PythonScriptEngine::initialize(DebugSession* session) {
    session_ = session;
    s_currentEngine = this;

    if (!Py_IsInitialized()) {
        PyImport_AppendInittab("edb", PyInit_edb);
        Py_Initialize();
        if (!Py_IsInitialized()) {
            return false;
        }
    } else {
        // Module might already be in inittab, make sure edb is imported
        PyObject* mod = PyImport_ImportModule("edb");
        if (!mod) {
            PyObject* m = PyInit_edb();
            if (m) {
                PyObject* sysMods = PyImport_GetModuleDict();
                PyDict_SetItemString(sysMods, "edb", m);
                Py_DECREF(m);
            }
        } else {
            Py_DECREF(mod);
        }
    }

    // Set up stdout & stderr redirector
    const char* redirectCode =
        "import sys, io, edb\n"
        "class _EdbOutputCatcher:\n"
        "    def __init__(self):\n"
        "        self.buf = []\n"
        "    def write(self, s):\n"
        "        self.buf.append(str(s))\n"
        "    def flush(self):\n"
        "        pass\n"
        "    def getvalue(self):\n"
        "        res = ''.join(self.buf)\n"
        "        self.buf.clear()\n"
        "        return res\n"
        "__edb_stdout__ = _EdbOutputCatcher()\n"
        "__edb_stderr__ = _EdbOutputCatcher()\n"
        "sys.stdout = __edb_stdout__\n"
        "sys.stderr = __edb_stderr__\n";

    PyRun_SimpleString(redirectCode);

    initialized_ = true;
    return true;
}

void PythonScriptEngine::shutdown() {
    if (s_currentEngine == this) {
        s_currentEngine = nullptr;
    }
    // Py_Finalize() is typically called only on complete app exit,
    // as re-initializing Python in the same process has known caveats in CPython.
    initialized_ = false;
}

ScriptResult PythonScriptEngine::executeString(const std::string& code) {
    if (!initialized_) {
        if (!initialize(session_)) {
            return {false, "", "Failed to initialize Python 3 engine."};
        }
    }

    s_currentEngine = this;

    PyObject* mainMod = PyImport_AddModule("__main__");
    PyObject* mainDict = PyModule_GetDict(mainMod);

    // Make sure edb module is accessible in global namespace
    PyObject* edbMod = PyImport_ImportModule("edb");
    if (edbMod) {
        PyDict_SetItemString(mainDict, "edb", edbMod);
        Py_DECREF(edbMod);
    }

    // Clear output buffers before execution
    PyRun_SimpleString("__edb_stdout__.buf.clear()\n__edb_stderr__.buf.clear()\n");

    // Execute code
    PyObject* result = PyRun_String(code.c_str(), Py_file_input, mainDict, mainDict);

    ScriptResult res;
    if (result == nullptr) {
        // Exception occurred, print traceback into sys.stderr
        PyErr_Print();
        res.success = false;
    } else {
        res.success = true;
        Py_DECREF(result);
    }

    // Retrieve stdout
    PyObject* outObj = PyRun_String("__edb_stdout__.getvalue()", Py_eval_input, mainDict, mainDict);
    if (outObj) {
        if (PyUnicode_Check(outObj)) {
            res.output = PyUnicode_AsUTF8(outObj);
        }
        Py_DECREF(outObj);
    }

    // Retrieve stderr
    PyObject* errObj = PyRun_String("__edb_stderr__.getvalue()", Py_eval_input, mainDict, mainDict);
    if (errObj) {
        if (PyUnicode_Check(errObj)) {
            res.error = PyUnicode_AsUTF8(errObj);
        }
        Py_DECREF(errObj);
    }

    if (!res.error.empty() && res.success) {
        // There were stderr warnings/prints, but code finished successfully
    }

    return res;
}

ScriptResult PythonScriptEngine::executeFile(const std::string& filepath) {
    std::string code = "with open(r'''" + filepath + "''', 'r', encoding='utf-8') as _f:\n"
                       "    exec(_f.read(), globals())\n";
    return executeString(code);
}

} // namespace edb_next
