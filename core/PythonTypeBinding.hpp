#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <functional>
#include <stdexcept>
#include <cstdint>

namespace edb_next::python_binding {

// Zero-overhead RAII Smart Pointer for PyObject*
template <typename T = PyObject>
class PyRef {
public:
    PyRef() : ptr_(nullptr) {}
    explicit PyRef(T* ptr) : ptr_(ptr) {}
    ~PyRef() {
        if (ptr_) {
            Py_DECREF(reinterpret_cast<PyObject*>(ptr_));
        }
    }

    PyRef(const PyRef& other) : ptr_(other.ptr_) {
        if (ptr_) {
            Py_INCREF(reinterpret_cast<PyObject*>(ptr_));
        }
    }

    PyRef& operator=(const PyRef& other) {
        if (this != &other) {
            if (ptr_) Py_DECREF(reinterpret_cast<PyObject*>(ptr_));
            ptr_ = other.ptr_;
            if (ptr_) Py_INCREF(reinterpret_cast<PyObject*>(ptr_));
        }
        return *this;
    }

    PyRef(PyRef&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    PyRef& operator=(PyRef&& other) noexcept {
        if (this != &other) {
            if (ptr_) Py_DECREF(reinterpret_cast<PyObject*>(ptr_));
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    T* release() noexcept {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

    void reset(T* ptr = nullptr) {
        if (ptr_) Py_DECREF(reinterpret_cast<PyObject*>(ptr_));
        ptr_ = ptr;
    }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }

private:
    T* ptr_{nullptr};
};

// Type traits for extracting arguments from PyObject* tuple
template <typename T>
struct PyArgReader;

template <>
struct PyArgReader<int> {
    static int read(PyObject* item) {
        if (!item || !PyLong_Check(item)) throw std::invalid_argument("Expected integer argument");
        return static_cast<int>(PyLong_AsLong(item));
    }
};

template <>
struct PyArgReader<uint64_t> {
    static uint64_t read(PyObject* item) {
        if (!item) throw std::invalid_argument("Expected integer argument");
        if (PyLong_Check(item)) {
            unsigned long long val = PyLong_AsUnsignedLongLong(item);
            if (val == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
                PyErr_Clear();
                long long sval = PyLong_AsLongLong(item);
                if (sval == -1 && PyErr_Occurred()) {
                    PyErr_Clear();
                    throw std::invalid_argument("Invalid integer conversion");
                }
                return static_cast<uint64_t>(sval);
            }
            return val;
        }
        throw std::invalid_argument("Expected integer argument");
    }
};

template <>
struct PyArgReader<std::string> {
    static std::string read(PyObject* item) {
        if (!item) return "";
        if (PyUnicode_Check(item)) {
            Py_ssize_t len = 0;
            const char* s = PyUnicode_AsUTF8AndSize(item, &len);
            return s ? std::string(s, len) : "";
        }
        if (PyBytes_Check(item)) {
            char* s = nullptr;
            Py_ssize_t len = 0;
            PyBytes_AsStringAndSize(item, &s, &len);
            return s ? std::string(s, len) : "";
        }
        throw std::invalid_argument("Expected string argument");
    }
};

template <>
struct PyArgReader<std::string_view> {
    static std::string_view read(PyObject* item) {
        if (!item) return "";
        if (PyUnicode_Check(item)) {
            Py_ssize_t len = 0;
            const char* s = PyUnicode_AsUTF8AndSize(item, &len);
            return s ? std::string_view(s, len) : "";
        }
        if (PyBytes_Check(item)) {
            char* s = nullptr;
            Py_ssize_t len = 0;
            PyBytes_AsStringAndSize(item, &s, &len);
            return s ? std::string_view(s, len) : "";
        }
        throw std::invalid_argument("Expected string or bytes argument");
    }
};

template <>
struct PyArgReader<bool> {
    static bool read(PyObject* item) {
        if (!item) return false;
        return PyObject_IsTrue(item) != 0;
    }
};

template <typename T>
struct PyArgReader<std::optional<T>> {
    static std::optional<T> read(PyObject* item) {
        if (!item || item == Py_None) {
            return std::nullopt;
        }
        return PyArgReader<T>::read(item);
    }
};

// Type traits for converting C++ values to PyObject*
inline PyObject* toPyObject() {
    Py_RETURN_NONE;
}

inline PyObject* toPyObject(bool v) {
    if (v) { Py_RETURN_TRUE; }
    else { Py_RETURN_FALSE; }
}

inline PyObject* toPyObject(int v) {
    return PyLong_FromLong(v);
}

inline PyObject* toPyObject(uint64_t v) {
    return PyLong_FromUnsignedLongLong(v);
}

inline PyObject* toPyObject(const std::string& v) {
    return PyUnicode_FromStringAndSize(v.data(), v.size());
}

inline PyObject* toPyObject(std::string_view v) {
    return PyUnicode_FromStringAndSize(v.data(), v.size());
}

inline PyObject* toPyObject(const char* v) {
    if (!v) { Py_RETURN_NONE; }
    return PyUnicode_FromString(v);
}

template <typename T>
inline PyObject* toPyObject(const std::optional<T>& v) {
    if (v.has_value()) {
        return toPyObject(*v);
    }
    Py_RETURN_NONE;
}

inline PyObject* toPyObject(const std::vector<uint8_t>& bytes) {
    return PyBytes_FromStringAndSize(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

inline PyObject* toPyObject(const std::vector<std::pair<std::string, uint64_t>>& items) {
    PyObject* dict = PyDict_New();
    if (!dict) return nullptr;
    for (const auto& [k, v] : items) {
        PyObject* val = PyLong_FromUnsignedLongLong(v);
        PyDict_SetItemString(dict, k.c_str(), val);
        Py_DECREF(val);
    }
    return dict;
}

template <typename F, typename Tuple, size_t... Is>
auto invokePyHelper(F&& f, Tuple&& args, std::index_sequence<Is...>) {
    return std::invoke(std::forward<F>(f), std::get<Is>(std::forward<Tuple>(args))...);
}

template <typename Ret, typename... Args>
class PythonFunctionDispatcher {
private:
    template <typename Func, size_t... Is>
    static PyObject* dispatchImpl(PyObject* args, Func&& func, std::index_sequence<Is...>) {
        Py_ssize_t numGiven = args ? PyTuple_Size(args) : 0;
        auto getArgItem = [&](size_t idx) -> PyObject* {
            if (static_cast<Py_ssize_t>(idx) < numGiven) {
                return PyTuple_GetItem(args, static_cast<Py_ssize_t>(idx));
            }
            return nullptr;
        };
        (void)getArgItem;

        std::tuple<std::decay_t<Args>...> argsTuple{
            PyArgReader<std::decay_t<Args>>::read(getArgItem(Is))...
        };

        if constexpr (std::is_void_v<Ret>) {
            invokePyHelper(std::forward<Func>(func), std::move(argsTuple), std::index_sequence_for<Args...>{});
            Py_RETURN_NONE;
        } else {
            auto ret = invokePyHelper(std::forward<Func>(func), std::move(argsTuple), std::index_sequence_for<Args...>{});
            return toPyObject(ret);
        }
    }

public:
    template <typename Func>
    static PyObject* dispatch(PyObject* args, Func&& func) {
        try {
            return dispatchImpl(args, std::forward<Func>(func), std::index_sequence_for<Args...>{});
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            return nullptr;
        } catch (...) {
            PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
            return nullptr;
        }
    }
};

} // namespace edb_next::python_binding
