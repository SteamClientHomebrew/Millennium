#include "executor.h"
#include <core/py_controller/co_spawn.h>
#include <core/ffi/ffi.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fstream>
#include <sys/log.h>
#include <sys/locals.h>
#include <core/hooks/web_load.h>
#include <core/co_initialize/co_stub.h>

std::shared_ptr<PluginLoader> g_pluginLoader;

PyObject* GetUserSettings(PyObject* self, PyObject* args)
{
    // std::unique_ptr<SettingsStore> settingsStorePtr = std::make_unique<SettingsStore>();
    // const nlohmann::json settingsInfo = settingsStorePtr->GetSetting();

    // PyObject* resultBuffer = PyDict_New();

    // for (auto it = settingsInfo.begin(); it != settingsInfo.end(); ++it) 
    // {
    //     PyObject* key = PyUnicode_FromString(it.key().c_str());
    //     PyObject* value = PyUnicode_FromString(it.value().get<std::string>().c_str());

    //     PyDict_SetItem(resultBuffer, key, value);
    //     Py_DECREF(key);
    //     Py_DECREF(value);
    // }

    //return resultBuffer;
    Py_RETURN_NONE;
}

PyObject* SetUserSettings(PyObject* self, PyObject* args)
{
    // std::unique_ptr<SettingsStore> settingsStorePtr = std::make_unique<SettingsStore>();

    // const char* key;
    // const char* value;

    // if (!PyArg_ParseTuple(args, "ss", &key, &value)) 
    // {
    //     return NULL;
    // }

    // nlohmann::json settingsInfo = settingsStorePtr->GetSetting();
    // {
    //     settingsInfo[key] = value;
    // }
    // settingsStorePtr->SetSetting(settingsInfo.dump(4));

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

    std::map<std::string, JavaScript::Types> typeMap 
    {
        { "str", JavaScript::Types::String },
        { "bool", JavaScript::Types::Boolean },
        { "int", JavaScript::Types::Integer }
    };

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

            try 
            {
                params.push_back({ strValue, typeMap[valueType] });
            }
            catch (const std::exception&) 
            {
                PyErr_SetString(PyExc_TypeError, "Millennium's IPC can only handle [bool, str, int]");
                return NULL;
            }
        }
    }

    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) 
    {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));
    const std::string script = JavaScript::ConstructFunctionCall(pluginName.c_str(), methodName, params);

    return JavaScript::EvaluateFromSocket(
        // Check the the frontend code is actually loaded aside from SteamUI
        fmt::format(
            "if (typeof window !== 'undefined' && typeof window.MillenniumFrontEndError === 'undefined') {{ window.MillenniumFrontEndError = class MillenniumFrontEndError extends Error {{ constructor(message) {{ super(message); this.name = 'MillenniumFrontEndError'; }} }} }}"
            "if (typeof PLUGIN_LIST === 'undefined' || !PLUGIN_LIST?.['{}']) throw new window.MillenniumFrontEndError('frontend not loaded yet!');\n\n{}", 
            pluginName, 
            script
        ) 
    );
}

PyObject* GetVersionInfo(PyObject* self, PyObject* args) 
{ 
    return PyUnicode_FromString(MILLENNIUM_VERSION);
}

PyObject* GetSteamPath(PyObject* self, PyObject* args) 
{
    return PyUnicode_FromString(SystemIO::GetSteamPath().string().c_str()); 
}

PyObject* GetInstallPath(PyObject* self, PyObject* args) 
{
    return PyUnicode_FromString(SystemIO::GetInstallPath().string().c_str()); 
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
    const char* regexSelector = ".*"; // Default value if no second parameter is provided

    // Parse arguments: moduleItem is required, regexSelector is optional
    if (!PyArg_ParseTuple(args, "s|s", &moduleItem, &regexSelector)) 
    {
        return 0;
    }

    g_hookedModuleId++;
    auto path = SystemIO::GetSteamPath() / "steamui" / moduleItem;

    try 
    {
        WebkitHandler::get().m_hookListPtr->push_back({ path.generic_string(), std::regex(regexSelector), type, g_hookedModuleId });
    } 
    catch (const std::regex_error& e) 
    {
        LOG_ERROR("Attempted to add a browser module with invalid regex: {} ({})", regexSelector, e.what());
        return 0;
    }

    return g_hookedModuleId;
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
    PyObject* statusObj;
    const char* pluginName;
    PythonManager& manager = PythonManager::GetInstance();
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    if (!PyArg_ParseTuple(args, "sO", &pluginName, &statusObj))
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to parse parameters. expected [str, bool]");
        return NULL;
    }

    if (!PyBool_Check(statusObj))
    {
        PyErr_SetString(PyExc_TypeError, "Second argument must be a boolean");
        return NULL;
    }

    const bool newToggleStatus = PyObject_IsTrue(statusObj);
    settingsStore->TogglePluginStatus(pluginName, newToggleStatus);

    if (!newToggleStatus)
    {
        std::thread([pluginName, &manager] { manager.DestroyPythonInstance(pluginName); }).detach();
    }
    else
    {
        Logger.Log("requested to enable plugin [{}]", pluginName);
        std::thread([&manager] { g_pluginLoader->StartBackEnds(manager); }).detach();
    }

    CoInitializer::ReInjectFrontendShims();
    Py_RETURN_NONE;
}

PyObject* EmitReadyMessage(PyObject* self, PyObject* args) 
{ 
    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) 
    {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));

    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
    backendHandler.BackendLoaded({ pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_SUCCESS });

    return PyBool_FromLong(true);
}

PyMethodDef* GetMillenniumModule()
{
    static PyMethodDef moduleMethods[] = 
    {
        { "ready",                 EmitReadyMessage,                METH_NOARGS,  NULL },

        { "add_browser_css",       AddBrowserCss,                   METH_VARARGS, NULL },
        { "add_browser_js",        AddBrowserJs,                    METH_VARARGS, NULL },
        { "remove_browser_module", RemoveBrowserModule,             METH_VARARGS, NULL },

        { "get_user_settings",     GetUserSettings,                 METH_NOARGS,  NULL },
        { "set_user_settings_key", SetUserSettings,                 METH_VARARGS, NULL },
        { "version",               GetVersionInfo,                  METH_NOARGS,  NULL },
        { "steam_path",            GetSteamPath,                    METH_NOARGS,  NULL },
        { "get_install_path",      GetInstallPath,                  METH_NOARGS,  NULL },

        { "call_frontend_method",  (PyCFunction)CallFrontendMethod, METH_VARARGS | METH_KEYWORDS, NULL },

        { "change_plugin_status",  TogglePluginStatus,              METH_VARARGS, NULL },
        {NULL, NULL, 0, NULL} // Sentinel
    };

    return moduleMethods;
}

void SetPluginLoader(std::shared_ptr<PluginLoader> pluginLoader) 
{
    g_pluginLoader = pluginLoader;
}