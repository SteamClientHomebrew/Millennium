#include <iostream>
#include <Python.h>
#include "co_spawn.hpp"
#include <utilities/log.hpp>
#include <fmt/core.h>

extern "C" void custom_output(std::string pname, const char* message) {
    if (std::string(message) != "\n" && std::string(message) != " ") console.py(pname, message);
}

extern "C" void custom_err_output(std::string pname, const char* message)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); 
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
#endif
    std::cout << message;
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}

extern "C" {
    static PyObject* custom_stdout_write(PyObject* self, PyObject* args) {
        const char* message;
        if (!PyArg_ParseTuple(args, "s", &message)) { return NULL; }

        custom_output(plugin_manager::get().get_plugin_name(PyThreadState_Get()), message);
        return Py_BuildValue("");
    }

    static PyObject* custom_stderr_write(PyObject* self, PyObject* args) {
        const char* message;
        if (!PyArg_ParseTuple(args, "s", &message)) { return NULL; }

        custom_err_output(plugin_manager::get().get_plugin_name(PyThreadState_Get()), message);
        return Py_BuildValue("");
    }

    static PyMethodDef module_methods[] = {
        {"write", custom_stdout_write, METH_VARARGS, "Custom stdout write function"}, {NULL, NULL, 0, NULL}
    };

    static PyMethodDef stderr_methods[] = {
        {"write", custom_stderr_write, METH_VARARGS, "Custom stderr write function"}, {NULL, NULL, 0, NULL}
    };

    static struct PyModuleDef custom_stdout_module = { PyModuleDef_HEAD_INIT, "hook_stdout", NULL, -1, module_methods };
    static struct PyModuleDef custom_stderr_module = { PyModuleDef_HEAD_INIT, "hook_stderr", NULL, -1, stderr_methods };

    PyMODINIT_FUNC PyInit_custom_stderr(void) {
        return PyModule_Create(&custom_stderr_module);
    }

    PyMODINIT_FUNC PyInit_custom_stdout(void) {
        return PyModule_Create(&custom_stdout_module);
    }

    const void redirect_output() {
        PyObject* sys = PyImport_ImportModule("sys");

        PyObject_SetAttrString(sys, "stdout", PyImport_ImportModule("hook_stdout"));
        PyObject_SetAttrString(sys, "stderr", PyImport_ImportModule("hook_stderr"));
    }
}