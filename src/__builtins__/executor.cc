#include "executor.h"
#include <core/py_controller/co_spawn.hpp>
#include <core/ffi/ffi.hpp>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fstream>
#include <utilities/log.hpp>
#include <generic/stream_parser.h>
#include <generic/base.h>
#include <core/hooks/web_load.h>

PyObject* GetUserSettings(PyObject* self, PyObject* args)
{
    const auto SettingsStore = SystemIO::ReadJsonSync(SystemIO::GetSteamPath().string());
    PyObject* resultBuffer = PyDict_New();

    for (auto it = SettingsStore.begin(); it != SettingsStore.end(); ++it) 
    {
        PyObject* key = PyUnicode_FromString(it.key().c_str());
        PyObject* value = PyUnicode_FromString(it.value().get<std::string>().c_str());

        PyDict_SetItem(resultBuffer, key, value);
        Py_DECREF(key);
        Py_DECREF(value);
    }

    return resultBuffer;
}

PyObject* SetUserSettings(PyObject* self, PyObject* args)
{
    const char* key;
    const char* value;

    if (!PyArg_ParseTuple(args, "ss", &key, &value)) 
    {
        return NULL;
    }

    auto data = SystemIO::ReadJsonSync(SystemIO::GetSteamPath().string());
    data[key] = value;

    SystemIO::WriteFileSync(SystemIO::GetSteamPath().string(), data.dump(4));
    Py_RETURN_NONE;
}

PyObject* CallFrontendMethod(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* methodName = NULL;
    PyObject* parameterList = NULL;

    static const char* keywordArgsList[] = { "method_name", "params", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", (char**)keywordArgsList, &methodName, &parameterList)) 
    {
        return NULL;
    }

    std::vector<JavaScript::JsFunctionConstructTypes> params;

    if (parameterList != NULL)
    {
        if (!PyList_Check(parameterList))
        {
            PyErr_SetString(PyExc_TypeError, "params must be a list");
            return NULL;
        }

        Py_ssize_t listSize = PyList_Size(parameterList);

        for (Py_ssize_t i = 0; i < listSize; ++i) 
        {
            PyObject* listItem = PyList_GetItem(parameterList, i);
            const std::string strValue  = PyUnicode_AsUTF8(PyObject_Str(listItem));
            const std::string valueType = Py_TYPE(listItem)->tp_name;

            if (valueType != "str" && valueType != "bool" && valueType != "int")
            {
                PyErr_SetString(PyExc_TypeError, "Millennium's IPC can only handle [bool, str, int]");
                return NULL;
            }

            params.push_back({ strValue, valueType });
        }
    }

    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) 
    {
        Logger.Error("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));
    const std::string script = JavaScript::ConstructFunctionCall(pluginName.c_str(), methodName, params);

    return JavaScript::EvaluateFromSocket(script);
}

PyObject* GetVersionInfo(PyObject* self, PyObject* args) 
{ 
    return PyUnicode_FromString(g_millenniumVersion); 
}

PyObject* GetSteamPath(PyObject* self, PyObject* args) 
{
    return PyUnicode_FromString(SystemIO::GetSteamPath().string().c_str()); 
}

PyObject* RemoveBrowserModule(PyObject* self, PyObject* args) 
{ 
    int moduleId;

    if (!PyArg_ParseTuple(args, "i", &moduleId)) 
    {
        return NULL;
    }

    bool success = false;
    const auto moduleList = WebkitHandler::get().m_hookListPtr;

    for (auto it = moduleList->begin(); it != moduleList->end();)
    {
        if (it->id == moduleId) 
        {
            it = moduleList->erase(it);
            success = true;
        } 
        else ++it;
    }
    return PyBool_FromLong(success);
}

unsigned long long AddBrowserModule(PyObject* args, WebkitHandler::TagTypes type) 
{
    const char* moduleItem;

    if (!PyArg_ParseTuple(args, "s", &moduleItem)) 
    {
        return 0;
    }

    hook_tag++;
    auto path = SystemIO::GetSteamPath() / "steamui" / moduleItem;

    WebkitHandler::get().m_hookListPtr->push_back({ path.generic_string(), type, hook_tag });
    return hook_tag;
}

PyObject* AddBrowserCss(PyObject* self, PyObject* args) 
{ 
    return PyLong_FromLong((long)AddBrowserModule(args, WebkitHandler::TagTypes::STYLESHEET)); 
}

PyObject* AddBrowserJs(PyObject* self, PyObject* args)  
{ 
    return PyLong_FromLong((long)AddBrowserModule(args, WebkitHandler::TagTypes::JAVASCRIPT)); 
}

/* 
This portion of the API is undocumented but you can use it. 
*/
PyObject* TogglePluginStatus(PyObject* self, PyObject* args) 
{ 
    Logger.Log("updating a plugins status.");

    const char* pluginName;
    PyObject* statusObj;
    int newToggleStatus;

    if (!PyArg_ParseTuple(args, "sO", &pluginName, &statusObj)) 
    {
        return NULL; // Parsing failed
    }

    if (!PyBool_Check(statusObj)) 
    {
        PyErr_SetString(PyExc_TypeError, "Second argument must be a boolean");
        return NULL;
    }

    newToggleStatus = PyObject_IsTrue(statusObj);

    if (!newToggleStatus) 
    {
        Logger.Log("requested to shutdown plugin [{}]", pluginName);
        std::thread(std::bind(&PythonManager::ShutdownPlugin, &PythonManager::GetInstance(), pluginName)).detach();
    }
    else 
    {
        Logger.Log("requested to enable plugin [{}]", pluginName);
    }

    Py_RETURN_NONE;
}

PyMethodDef* GetMillenniumModule()
{
    static PyMethodDef moduleMethods[] = 
    {
        { "add_browser_css", AddBrowserCss, METH_VARARGS, NULL },
        { "add_browser_js", AddBrowserJs, METH_VARARGS, NULL },
        { "remove_browser_module", RemoveBrowserModule, METH_VARARGS, NULL },

        { "get_user_settings", GetUserSettings, METH_NOARGS, NULL },
        { "set_user_settings_key", SetUserSettings, METH_VARARGS, NULL },
        { "version", GetVersionInfo, METH_NOARGS, NULL },
        { "steam_path", GetSteamPath, METH_NOARGS, NULL },
        { "call_frontend_method", (PyCFunction)CallFrontendMethod, METH_VARARGS | METH_KEYWORDS, NULL },

        { "change_plugin_status", TogglePluginStatus, METH_VARARGS, NULL },
        {NULL, NULL, 0, NULL} // Sentinel
    };

    return moduleMethods;
}
