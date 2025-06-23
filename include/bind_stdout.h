/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * bind_stdout.h
 * @brief This file is responsible for redirecting the Python stdout and stderr to the logger.
 * @note You shouldn't rely print() in python, use "PluginUtils" utils module, this is just a catch-all for any print() calls that may be made.
 */

#include <iostream>
#include <Python.h>
#include "co_spawn.h"
#include "internal_logger.h"
#include <fmt/core.h>
#include "plugin_logger.h"

/**
 * @brief Write a message buffer to the logger. The buffer is un-flushed. 
 * 
 * @param pname The name of the plugin.
 * @param message The message to be printed.
 */
extern "C" void PrintPythonMessage(std::string pname, const char* message) 
{
    const std::string logMessage = message;

    /** Dont process new lines or empty spaces (which seem to represent empty lines) */
    if (logMessage != "\n" && logMessage != " ")
    {
        Logger.LogPluginMessage(pname, message);
        InfoToLogger(pname, message);
    }
}

/**
 * @brief Write an error message buffer to the logger. 
 * We dont use the plugin here as most messages coming from stderr are flushed every character.
 * 
 * ex. 123 would print plugin name 1\n plugin name 2\n plugin name 3\n
 */
extern "C" void PrintPythonError(std::string pname, const char* message)
{
    std::cout << "\033[31m" << message << "\033[0m"; 
    std::cout.flush(); // Flush the buffer to ensure the message is printed immediately, and the color doesn't leak into the next message.

    ErrorToLogger(pname, message);
}

/**
 * @brief Redirects the Python stdout and stderr to the logger.
 */
#define HOOK_OUT_WRITE(write_function) \
    const char* message; \
    if (!PyArg_ParseTuple(args, "s", &message)) \
    { \
        return NULL; \
    } \
    write_function(PythonManager::GetInstance().GetPluginNameFromThreadState(PyThreadState_Get()), message); \
    return Py_BuildValue("");

extern "C" 
{
    /** Forward messages to respective logger type. */
    static PyObject* CustomStdoutWrite(PyObject* self, PyObject* args) { HOOK_OUT_WRITE(PrintPythonMessage); }
    static PyObject* CustomStderrWrite(PyObject* self, PyObject* args) { HOOK_OUT_WRITE(PrintPythonError);   }

    static PyMethodDef stdoutMethods[] = { {"write", CustomStdoutWrite, METH_VARARGS, "Custom stdout write function"}, {NULL, NULL, 0, NULL} };
    static PyMethodDef stderrMethods[] = { {"write", CustomStderrWrite, METH_VARARGS, "Custom stderr write function"}, {NULL, NULL, 0, NULL} };

    static struct PyModuleDef customStdoutModule = { PyModuleDef_HEAD_INIT, "hook_stdout", NULL, -1, stdoutMethods };
    static struct PyModuleDef customStderrModule = { PyModuleDef_HEAD_INIT, "hook_stderr", NULL, -1, stderrMethods };

    PyObject* PyInit_CustomStderr(void) { return PyModule_Create(&customStderrModule); }
    PyObject* PyInit_CustomStdout(void) { return PyModule_Create(&customStdoutModule); }

    /** @brief Redirects the Python stdout and stderr to the logger. */
    const void RedirectOutput() 
    {
        PyObject* sys = PyImport_ImportModule("sys");

        PyObject_SetAttrString(sys, "stdout", PyImport_ImportModule("hook_stdout"));
        PyObject_SetAttrString(sys, "stderr", PyImport_ImportModule("hook_stderr"));
    }
}