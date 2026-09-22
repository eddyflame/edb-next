#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "PythonScriptEngine.hpp"
#include "ScriptApiBridge.hpp"
#include "PythonTypeBinding.hpp"
#include "LogManager.hpp"

#include <iostream>
#include <sstream>

namespace edb_next {

static thread_local PythonScriptEngine* t_currentEngine = nullptr;

struct PythonEngineScope {
    PythonScriptEngine* prev_;
    explicit PythonEngineScope(PythonScriptEngine* eng) : prev_(t_currentEngine) {
        t_currentEngine = eng;
    }
    ~PythonEngineScope() {
        t_currentEngine = prev_;
    }
};

class GilStateScope {
public:
    GilStateScope() : state_(PyGILState_Ensure()) {}
    ~GilStateScope() { PyGILState_Release(state_); }
    GilStateScope(const GilStateScope&) = delete;
    GilStateScope& operator=(const GilStateScope&) = delete;
private:
    PyGILState_STATE state_;
};

namespace {

static PyObject* py_read_memory(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<std::vector<uint8_t>, uint64_t, size_t>::dispatch(args, [](uint64_t addr, size_t size) {
        return ScriptApiBridge::readMemory(PythonScriptEngine::activeSession(), addr, size);
    });
}

static PyObject* py_write_memory(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<bool, uint64_t, std::string_view>::dispatch(args, [](uint64_t addr, std::string_view data) {
        return ScriptApiBridge::writeMemory(PythonScriptEngine::activeSession(), addr, data.data(), data.size());
    });
}

static PyObject* py_get_reg(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<std::optional<uint64_t>, std::string>::dispatch(args, [](const std::string& reg) {
        return ScriptApiBridge::getReg(PythonScriptEngine::activeSession(), reg);
    });
}

static PyObject* py_set_reg(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<bool, std::string, uint64_t>::dispatch(args, [](const std::string& reg, uint64_t val) {
        return ScriptApiBridge::setReg(PythonScriptEngine::activeSession(), reg, val);
    });
}

static PyObject* py_get_regs(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<std::vector<std::pair<std::string, uint64_t>>>::dispatch(args, []() {
        return ScriptApiBridge::getRegs(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_set_breakpoint(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<bool, uint64_t, std::optional<std::string>>::dispatch(args, [](uint64_t addr, std::optional<std::string> sym) {
        return ScriptApiBridge::setBreakpoint(PythonScriptEngine::activeSession(), addr, sym.value_or(""));
    });
}

static PyObject* py_remove_breakpoint(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<bool, uint64_t>::dispatch(args, [](uint64_t addr) {
        return ScriptApiBridge::removeBreakpoint(PythonScriptEngine::activeSession(), addr);
    });
}

static PyObject* py_step_into(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<void>::dispatch(args, []() {
        ScriptApiBridge::stepInto(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_step_over(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<void>::dispatch(args, []() {
        ScriptApiBridge::stepOver(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_step_source(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<void>::dispatch(args, []() {
        ScriptApiBridge::stepSource(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_resume(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<void>::dispatch(args, []() {
        ScriptApiBridge::resume(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_pause(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<void>::dispatch(args, []() {
        ScriptApiBridge::pause(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_resolve_symbol(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<std::optional<uint64_t>, std::string>::dispatch(args, [](const std::string& name) {
        return ScriptApiBridge::resolveSymbol(PythonScriptEngine::activeSession(), name);
    });
}

static PyObject* py_eval(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<std::optional<uint64_t>, std::string>::dispatch(args, [](const std::string& expr) {
        return ScriptApiBridge::eval(PythonScriptEngine::activeSession(), expr);
    });
}

static PyObject* py_pid(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<int>::dispatch(args, []() {
        return ScriptApiBridge::pid(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_tid(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<int>::dispatch(args, []() {
        return ScriptApiBridge::tid(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_state(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<std::string>::dispatch(args, []() {
        return ScriptApiBridge::state(PythonScriptEngine::activeSession());
    });
}

static PyObject* py_log(PyObject* /*self*/, PyObject* args) {
    using namespace python_binding;
    return PythonFunctionDispatcher<void, std::string>::dispatch(args, [](const std::string& msg) {
        ScriptApiBridge::log("Python", msg);
    });
}

static PyMethodDef EdbMethods[] = {
    {"read_memory", py_read_memory, METH_VARARGS, "Read memory: read_memory(addr, size) -> bytes"},
    {"write_memory", py_write_memory, METH_VARARGS, "Write memory: write_memory(addr, bytes) -> bool"},
    {"get_reg", py_get_reg, METH_VARARGS, "Get register value: get_reg(name) -> int or None"},
    {"set_reg", py_set_reg, METH_VARARGS, "Set register value: set_reg(name, val) -> bool"},
    {"get_regs", py_get_regs, METH_VARARGS, "Get all registers: get_regs() -> dict"},
    {"set_breakpoint", py_set_breakpoint, METH_VARARGS, "Set breakpoint: set_breakpoint(addr, symbol='') -> bool"},
    {"remove_breakpoint", py_remove_breakpoint, METH_VARARGS, "Remove breakpoint: remove_breakpoint(addr) -> bool"},
    {"step_into", py_step_into, METH_VARARGS, "Step into instruction"},
    {"step_over", py_step_over, METH_VARARGS, "Step over instruction"},
    {"step_source", py_step_source, METH_VARARGS, "Step source line"},
    {"resume", py_resume, METH_VARARGS, "Resume execution"},
    {"pause", py_pause, METH_VARARGS, "Pause execution"},
    {"resolve_symbol", py_resolve_symbol, METH_VARARGS, "Resolve symbol name: resolve_symbol(name) -> int or None"},
    {"eval", py_eval, METH_VARARGS, "Evaluate expression: eval(expr) -> int or None"},
    {"pid", py_pid, METH_VARARGS, "Get target process PID: pid() -> int"},
    {"tid", py_tid, METH_VARARGS, "Get active thread TID: tid() -> int"},
    {"state", py_state, METH_VARARGS, "Get session state: state() -> str"},
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
    if (t_currentEngine) {
        return t_currentEngine->session_;
    }
    return nullptr;
}

void PythonScriptEngine::setSession(DebugSession* session) {
    session_ = session;
}

bool PythonScriptEngine::initialize(DebugSession* session) {
    session_ = session;
    PythonEngineScope scope(this);

    if (!Py_IsInitialized()) {
        PyImport_AppendInittab("edb", PyInit_edb);
        Py_Initialize();
        if (!Py_IsInitialized()) {
            return false;
        }
    } else {
        GilStateScope gil;
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

    GilStateScope gil;
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
    if (t_currentEngine == this) {
        t_currentEngine = nullptr;
    }
    initialized_ = false;
}

ScriptResult PythonScriptEngine::executeString(const std::string& code) {
    if (!initialized_) {
        if (!initialize(session_)) {
            return {false, "", "Failed to initialize Python 3 engine."};
        }
    }

    PythonEngineScope scope(this);
    GilStateScope gil;

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

    return res;
}

ScriptResult PythonScriptEngine::executeFile(const std::string& filepath) {
    std::string code = "with open(r'''" + filepath + "''', 'r', encoding='utf-8') as _f:\n"
                       "    exec(_f.read(), globals())\n";
    return executeString(code);
}

bool PythonScriptEngine::executeHook(const std::string& code) {
    if (!initialized_) {
        if (!initialize(session_)) {
            return true;
        }
    }

    PythonEngineScope scope(this);
    GilStateScope gil;

    std::string wrapped = "def __edb_bp_hook__():\n";
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        wrapped += "    " + line + "\n";
    }
    wrapped += "__edb_hook_ret__ = __edb_bp_hook__()\n";

    PyObject* mainMod = PyImport_AddModule("__main__");
    PyObject* mainDict = PyModule_GetDict(mainMod);

    PyObject* edbMod = PyImport_ImportModule("edb");
    if (edbMod) {
        PyDict_SetItemString(mainDict, "edb", edbMod);
        Py_DECREF(edbMod);
    }

    PyObject* res = PyRun_String(wrapped.c_str(), Py_file_input, mainDict, mainDict);
    if (!res) {
        PyErr_Print();
        return true;
    }
    Py_DECREF(res);

    PyObject* retVal = PyDict_GetItemString(mainDict, "__edb_hook_ret__");
    if (retVal == Py_False) {
        return false;
    }
    return true;
}

} // namespace edb_next
