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

#include "plugin_logger.h"
#include <Python.h>
#include <stdio.h>
#include <fstream>


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
PyObject* LoggerObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    LoggerObject *self;
    self = (LoggerObject *)type->tp_alloc(type, 0);
    if (!self) 
    {
        return NULL;
    }

    const char* prefix = NULL;
    if (PyArg_ParseTuple(args, "|s", &prefix) && prefix != NULL) 
    {
        PyErr_WarnEx(PyExc_DeprecationWarning, "DEVELOPER INFO: Logger() no longer accepts custom name parameters, this is not fatal however all parameters will be ignored, please remove them.", 1);
    }

    PyObject* builtins = PyEval_GetBuiltins();
    if (!builtins) 
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve __builtins__.");
        Py_DECREF(self);
        return NULL;
    }

    /** Get the variable from the __builtins__ dictionary */
    PyObject* value = PyDict_GetItemString(builtins, "MILLENNIUM_PLUGIN_SECRET_NAME");
    std::string pluginName = value ? PyUnicode_AsUTF8(value) : "ERRNO_PLUGIN_NAME";

    /** Check if the logger already exists, and use it if it does */
    for (auto logger : g_loggerList) 
    {
        if (logger->GetPluginName(false) == pluginName) 
        {
            self->m_loggerPtr = logger;
            return (PyObject *)self;
        }
    }

    self->m_loggerPtr = new BackendLogger(pluginName);
    if (!self->m_loggerPtr) 
    {
        Py_DECREF(self);
        return NULL;
    }
    
    g_loggerList.push_back(self->m_loggerPtr);
    return (PyObject *)self;
}

/**
 * @brief Deallocates a LoggerObject instance.
 * 
 * This function deallocates a LoggerObject instance and does not delete the logger since it's shared in g_loggerList.
 * 
 * @param {LoggerObject*} self - The LoggerObject instance to deallocate.
 */
void LoggerObject_dealloc(LoggerObject *self)
{
    /** Don't delete the logger here since it's shared in g_loggerList */
    Py_TYPE(self)->tp_free((PyObject *)self);
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
PyObject* LoggerObject_log(LoggerObject *self, PyObject *args)
{
    const char *message;
    if (!PyArg_ParseTuple(args, "s", &message)) 
    {
        return NULL;
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
PyObject* LoggerObject_error(LoggerObject *self, PyObject *args)
{
    const char *message;
    if (!PyArg_ParseTuple(args, "s", &message)) 
    {
        return NULL;
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
PyObject* LoggerObject_warning(LoggerObject *self, PyObject *args)
{
    const char *message;
    if (!PyArg_ParseTuple(args, "s", &message)) 
    {
        return NULL;
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
static PyMethodDef LoggerObject_methods[] = 
{
    {"log",     (PyCFunction)LoggerObject_log,   METH_VARARGS, "Log a message"        },
    {"error",   (PyCFunction)LoggerObject_error, METH_VARARGS, "Log an error message" },
    {"warn", (PyCFunction)LoggerObject_warning,  METH_VARARGS, "Log a warning message"},

    {NULL, NULL, 0, NULL}  /* Sentinel */
};

/**
 * @brief The type object for the LoggerObject instance.
 * 
 * This object defines the type for the LoggerObject instance.
 * 
 * @returns {PyTypeObject*} - A pointer to the type object for the LoggerObject instance.
 */
PyTypeObject LoggerType 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "logger.Logger",
    .tp_basicsize = sizeof(LoggerObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)LoggerObject_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Logger object",
    .tp_methods = LoggerObject_methods,
    .tp_new = LoggerObject_new,
};

/**
 * @brief The module definition for the logger module.
 * 
 * This object defines the module for the logger module.
 * 
 * @returns {PyModuleDef*} - A pointer to the module definition for the logger module.
 */
static struct PyModuleDef g_loggerModuleDef 
{
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "logger",
    .m_doc = "Millennium logger module",
    .m_size = -1,
    .m_methods = LoggerObject_methods,
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
    if (PyType_Ready(&LoggerType) < 0)
    {
        /** Failing indicates a serious initialization error */
        return NULL;
    }

    /** Create the logger module */
    PyObject *loggerModule = PyModule_Create(&g_loggerModuleDef);
    if (loggerModule == NULL)
    {
        return NULL;
    }

    Py_INCREF(&LoggerType);
    if (PyModule_AddObject(loggerModule, "Logger", (PyObject *)&LoggerType) < 0) 
    {
        Py_DECREF(&LoggerType);
        Py_DECREF(loggerModule);
        return NULL;
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
    for (auto logger : g_loggerList) {
        delete logger;
    }
    g_loggerList.clear();
}
