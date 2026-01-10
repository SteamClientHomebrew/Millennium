/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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

#include "millennium/plugin_logger.h"

#include <Python.h>

std::vector<BackendLogger*> g_loggerList;

/**
 * @brief Creates a new LoggerObject instance.
 *
 * This function creates a new LoggerObject instance and initializes it with the given arguments.
 *
 * @param {PyTypeObject*} type - The type of the object to create.
 * @param {PyObject*} args - The arguments to pass to the constructor.
 * @param {PyObject*} kwds - The keyword arguments to pass to the constructor.
 *
 * @returns {PyObject*} - A pointer to the new LoggerObject instance.
 */
PyObject* LoggerObject_new(PyTypeObject* type, PyObject* args, PyObject* /** kwds */)
{
    LoggerObject * self = reinterpret_cast<LoggerObject*>(type->tp_alloc(type, 0));
    if (!self) {
        return nullptr;
    }

    const char* prefix = nullptr;
    if (PyArg_ParseTuple(args, "|s", &prefix) && prefix != nullptr) {
        PyErr_WarnEx(PyExc_DeprecationWarning,
                     "DEVELOPER INFO: Logger() no longer accepts custom name parameters, this is not fatal however all parameters will be ignored, please remove them.", 1);
    }

    PyObject* builtins = PyEval_GetBuiltins();
    if (!builtins) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve __builtins__.");
        Py_DECREF(self);
        return nullptr;
    }

    /** Get the variable from the __builtins__ dictionary */
    PyObject* value = PyDict_GetItemString(builtins, "MILLENNIUM_PLUGIN_SECRET_NAME");
    const std::string pluginName = value ? PyUnicode_AsUTF8(value) : "ERRNO_PLUGIN_NAME";

    /** Check if the logger already exists, and use it if it does */
    for (const auto logger : g_loggerList) {
        if (logger->GetPluginName(false) == pluginName) {
            self->m_loggerPtr = logger;
            return reinterpret_cast<PyObject*>(self);
        }
    }

    self->m_loggerPtr = new BackendLogger(pluginName);
    if (!self->m_loggerPtr) {
        Py_DECREF(self);
        return nullptr;
    }

    g_loggerList.push_back(self->m_loggerPtr);
    return reinterpret_cast<PyObject*>(self);
}

/**
 * @brief Deallocates a LoggerObject instance.
 *
 * This function deallocates a LoggerObject instance and does not delete the logger since it's shared in g_loggerList.
 *
 * @param {LoggerObject*} self - The LoggerObject instance to deallocate.
 */
void LoggerObject_dealloc(LoggerObject* self)
{
    /** Don't delete the logger here since it's shared in g_loggerList */
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

/**
 * @brief Logs a message.
 *
 * This function logs a message using the LoggerObject instance.
 *
 * @param {LoggerObject*} self - The LoggerObject instance to log the message.
 * @param {PyObject*} args - The arguments to pass to the log method.
 *
 * @returns {PyObject*} - A pointer to the new LoggerObject instance.
 */
PyObject* LoggerObject_log(const LoggerObject* self, PyObject* args)
{
    const char* message;
    if (!PyArg_ParseTuple(args, "s", &message)) {
        return nullptr;
    }

    self->m_loggerPtr->Log(message);
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * @brief Logs an error message.
 *
 * This function logs an error message using the LoggerObject instance.
 *
 * @param {LoggerObject*} self - The LoggerObject instance to log the error message.
 * @param {PyObject*} args - The arguments to pass to the error method.
 *
 * @returns {PyObject*} - A pointer to the new LoggerObject instance.
 */
PyObject* LoggerObject_error(const LoggerObject* self, PyObject* args)
{
    const char* message;
    if (!PyArg_ParseTuple(args, "s", &message)) {
        return nullptr;
    }

    self->m_loggerPtr->Error(message);
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * @brief Logs a warning message.
 *
 * This function logs a warning message using the LoggerObject instance.
 *
 * @param {LoggerObject*} self - The LoggerObject instance to log the warning message.
 * @param {PyObject*} args - The arguments to pass to the warning method.
 *
 * @returns {PyObject*} - A pointer to the new LoggerObject instance.
 */
PyObject* LoggerObject_warning(const LoggerObject* self, PyObject* args)
{
    const char* message;
    if (!PyArg_ParseTuple(args, "s", &message)) {
        return nullptr;
    }

    self->m_loggerPtr->Warn(message);
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * @brief The methods for the LoggerObject instance.
 *
 * This array defines the methods for the LoggerObject instance.
 *
 * @returns {PyMethodDef*} - A pointer to the methods for the LoggerObject instance.
 */
static PyMethodDef LoggerObject_methods[] = {
    { "log",   reinterpret_cast<PyCFunction>(LoggerObject_log),     METH_VARARGS, "Log a message"         },
    { "error", reinterpret_cast<PyCFunction>(LoggerObject_error),   METH_VARARGS, "Log an error message"  },
    { "warn",  reinterpret_cast<PyCFunction>(LoggerObject_warning), METH_VARARGS, "Log a warning message" },

    { nullptr,    nullptr,                              0,            nullptr                    }  /* Sentinel */
};

/**
 * @brief The type object for the LoggerObject instance.
 *
 * This object defines the type for the LoggerObject instance.
 *
 * @returns {PyTypeObject*} - A pointer to the type object for the LoggerObject instance.
 */
PyTypeObject LoggerType = {
    PyVarObject_HEAD_INIT(nullptr, 0) "logger.Logger", // tp_name
    sizeof(LoggerObject),                                 // tp_basicsize
    0,                                                    // tp_itemsize
    reinterpret_cast<destructor>(LoggerObject_dealloc),   // tp_dealloc
    0,                                                    // tp_vectorcall_offset
    nullptr,                                              // tp_getattr
    nullptr,                                              // tp_setattr
    nullptr,                                              // tp_as_async
    nullptr,                                              // tp_repr
    nullptr,                                              // tp_as_number
    nullptr,                                              // tp_as_sequence
    nullptr,                                              // tp_as_mapping
    nullptr,                                              // tp_hash
    nullptr,                                              // tp_call
    nullptr,                                              // tp_str
    nullptr,                                              // tp_getattro
    nullptr,                                              // tp_setattro
    nullptr,                                              // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                                   // tp_flags
    "Logger object",                                      // tp_doc
    nullptr,                                              // tp_traverse
    nullptr,                                              // tp_clear
    nullptr,                                              // tp_richcompare
    0,                                                    // tp_weaklistoffset
    nullptr,                                              // tp_iter
    nullptr,                                              // tp_iternext
    LoggerObject_methods,                                 // tp_methods
    nullptr,                                              // tp_members
    nullptr,                                              // tp_getset
    nullptr,                                              // tp_base
    nullptr,                                              // tp_dict
    nullptr,                                              // tp_descr_get
    nullptr,                                              // tp_descr_set
    0,                                                    // tp_dictoffset
    nullptr,                                              // tp_init
    nullptr,                                              // tp_alloc
    LoggerObject_new,                                     // tp_new
    nullptr,                                              // tp_free
    nullptr,                                              // tp_is_gc
    nullptr,                                              // tp_bases
    nullptr,                                              // tp_mro
    nullptr,                                              // tp_cache
    nullptr,                                              // tp_subclasses
    nullptr,                                              // tp_weaklist
    nullptr,                                              // tp_del
    0,                                                    // tp_version_tag
    nullptr,                                              // tp_finalize
    nullptr,                                              // tp_vectorcall
};

/**
 * @brief The module definition for the logger module.
 *
 * This object defines the module for the logger module.
 *
 * @returns {PyModuleDef*} - A pointer to the module definition for the logger module.
 */
static PyModuleDef g_loggerModuleDef = {
    PyModuleDef_HEAD_INIT,   // m_base
    "logger",                   // m_name
    "Millennium logger module", // m_doc
    -1,                         // m_size
    LoggerObject_methods,       // m_methods
    nullptr,                    // m_slots
    nullptr,                    // m_traverse
    nullptr,                    // m_clear
    nullptr,                    // m_free
};

/**
 * @brief Initializes the logger module.
 *
 * This function initializes the logger module.
 *
 * @returns {PyObject*} - A pointer to the logger module.
 */
PyObject* PyInit_Logger(void)
{
    if (PyType_Ready(&LoggerType) < 0) {
        /** Failing indicates a serious initialization error */
        return nullptr;
    }

    /** Create the logger module */
    PyObject* loggerModule = PyModule_Create(&g_loggerModuleDef);
    if (loggerModule == nullptr) {
        return nullptr;
    }

    Py_INCREF(&LoggerType);
    if (PyModule_AddObject(loggerModule, "Logger", reinterpret_cast<PyObject*>(&LoggerType)) < 0) {
        Py_DECREF(&LoggerType);
        Py_DECREF(loggerModule);
        return nullptr;
    }

    return loggerModule;
}

/**
 * @brief Cleans up the global logger list.
 *
 * This function iterates through the global logger list and deletes each logger.
 * It then clears the list to ensure no memory leaks occur.
 */
void CleanupLoggers()
{
    for (const auto logger : g_loggerList) {
        delete logger;
    }
    g_loggerList.clear();
}
