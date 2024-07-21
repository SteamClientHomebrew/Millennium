#include <core/py_controller/logger.h>
#include <Python.h>
#include <stdio.h>
#include <fstream>

static PyObject* LoggerObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    LoggerObject *self;
    self = (LoggerObject *)type->tp_alloc(type, 0);

    if (self != NULL) 
    {
        self->prefix = NULL;

        if (!PyArg_ParseTuple(args, "|s", &self->prefix)) 
        {
            Py_DECREF(self);
            return NULL;
        }
    }

    self->m_loggerPtr = std::make_unique<BackendLogger>(self->prefix);
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
