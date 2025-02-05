#include <core/py_controller/logger.h>
#include <Python.h>
#include <stdio.h>
#include <fstream>

std::vector<BackendLogger*> g_loggerList;

static PyObject* LoggerObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    LoggerObject *self;
    self = (LoggerObject *)type->tp_alloc(type, 0);

    const char* prefix = NULL;
    if (PyArg_ParseTuple(args, "|s", &prefix) && prefix != NULL) 
    {
        PyErr_WarnEx(PyExc_DeprecationWarning, "DEVELOPER INFO: Logger() no longer accepts custom name parameters, this is not fatal however all parameters will be ignored, please remove them.", 1);
    }

    PyObject* builtins = PyEval_GetBuiltins();
    if (!builtins) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve __builtins__.");
        return NULL;
    }

    // Get the variable from the __builtins__ dictionary
    PyObject* value = PyDict_GetItemString(builtins, "MILLENNIUM_PLUGIN_SECRET_NAME");
    std::string pluginName = value ? PyUnicode_AsUTF8(value) : "ERRNO_PLUGIN_NAME";

    // Check if the logger already exists, and use it if it does
    for (auto logger : g_loggerList) 
    {
        if (logger->GetPluginName(false) == pluginName) 
        {
            self->m_loggerPtr = logger;
            return (PyObject *)self;
        }
    }

    self->m_loggerPtr = new BackendLogger(pluginName);
    g_loggerList.push_back(self->m_loggerPtr);
    return (PyObject *)self;
}

static void LoggerObject_dealloc(LoggerObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* LoggerObject_log(LoggerObject *self, PyObject *args)
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

static PyObject* LoggerObject_error(LoggerObject *self, PyObject *args)
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

static PyObject* LoggerObject_warning(LoggerObject *self, PyObject *args)
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

static PyMethodDef LoggerObject_methods[] = 
{
    {"log",     (PyCFunction)LoggerObject_log,   METH_VARARGS, "Log a message"        },
    {"error",   (PyCFunction)LoggerObject_error, METH_VARARGS, "Log an error message" },
    {"warn", (PyCFunction)LoggerObject_warning,  METH_VARARGS, "Log a warning message"},

    {NULL, NULL, 0, NULL}  /* Sentinel */
};

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

static struct PyModuleDef g_loggerModuleDef 
{
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "logger",
    .m_doc = "Millennium logger module",
    .m_size = -1,
    .m_methods = LoggerObject_methods,
};

PyObject* PyInit_Logger(void)
{
    PyObject *loggerModule;
    if (PyType_Ready(&LoggerType) < 0)
    {
        return NULL;
    }

    loggerModule = PyModule_Create(&g_loggerModuleDef);
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
