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

#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/encode.h"
#include "millennium/ffi.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/millennium_api.h"
#include "millennium/plugin_api_init.h"
#include "millennium/plugin_logger.h"
#include "millennium/sysfs.h"
#include <fmt/core.h>
#include <fstream>
#include <nlohmann/json.hpp>

PyObject* GetUserSettings(PyObject* self, PyObject* args)
{
    PyErr_SetString(PyExc_NotImplementedError, "get_user_settings is not implemented yet. It will likely be removed in the future.");
    return NULL;
}

PyObject* SetUserSettings(PyObject* self, PyObject* args)
{
    PyErr_SetString(PyExc_NotImplementedError, "set_user_settings_key is not implemented yet. It will likely be removed in the future.");
    return NULL;
}

PyObject* CallFrontendMethod(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* methodName = NULL;
    PyObject* parameterList = NULL;

    static const char* keywordArgsList[] = { "method_name", "params", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", (char**)keywordArgsList, &methodName, &parameterList)) {
        return NULL;
    }

    std::vector<JavaScript::JsFunctionConstructTypes> params;

    if (parameterList != NULL) {
        if (!PyList_Check(parameterList)) {
            PyErr_SetString(PyExc_TypeError, "params must be a list");
            return NULL;
        }

        Py_ssize_t listSize = PyList_Size(parameterList);

        for (Py_ssize_t i = 0; i < listSize; ++i) {
            PyObject* listItem = PyList_GetItem(parameterList, i);
            const std::string strValue = PyUnicode_AsUTF8(PyObject_Str(listItem));
            const std::string valueType = Py_TYPE(listItem)->tp_name;

            try {
                params.push_back({ strValue, typeMap[valueType] });
            } catch (const std::exception&) {
                PyErr_SetString(PyExc_TypeError, "Millennium's IPC can only handle [bool, str, int]");
                return NULL;
            }
        }
    }

    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));
    const std::string script = JavaScript::ConstructFunctionCall(pluginName.c_str(), methodName, params);

    return JavaScript::Py_EvaluateFromSocket(
        // Check the the frontend code is actually loaded aside from SteamUI
        fmt::format("if (typeof window !== 'undefined' && typeof window.MillenniumFrontEndError === 'undefined') {{ window.MillenniumFrontEndError = class MillenniumFrontEndError "
                    "extends Error {{ constructor(message) {{ super(message); this.name = 'MillenniumFrontEndError'; }} }} }}"
                    "if (typeof PLUGIN_LIST === 'undefined' || !PLUGIN_LIST?.['{}']) throw new window.MillenniumFrontEndError('frontend not loaded yet!');\n\n{}",
                    pluginName, script));
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

    if (!PyArg_ParseTuple(args, "i", &moduleId)) {
        return NULL;
    }

    const bool success = HttpHookManager::get().RemoveHook(moduleId);
    return PyBool_FromLong(success);
}

unsigned long long AddBrowserModule(PyObject* args, HttpHookManager::TagTypes type)
{
    const char* moduleItem;
    const char* regexSelector = ".*";

    if (!PyArg_ParseTuple(args, "s|s", &moduleItem, &regexSelector)) {
        return 0;
    }

    return Millennium_AddBrowserModule(moduleItem, regexSelector, type);
}

PyObject* AddBrowserCss(PyObject* self, PyObject* args)
{
    return PyLong_FromLong((long)AddBrowserModule(args, HttpHookManager::TagTypes::STYLESHEET));
}

PyObject* AddBrowserJs(PyObject* self, PyObject* args)
{
    return PyLong_FromLong((long)AddBrowserModule(args, HttpHookManager::TagTypes::JAVASCRIPT));
}

/**
 * A utility function to check if a plugin is enabled.
 */
PyObject* IsPluginEnable(PyObject* self, PyObject* args)
{
    const char* pluginName = NULL;

    if (!PyArg_ParseTuple(args, "s", &pluginName)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to parse parameters");
        return NULL;
    }

    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    bool isEnabled = settingsStore->IsEnabledPlugin(pluginName);
    return PyBool_FromLong(isEnabled);
}

PyObject* EmitReadyMessage(PyObject* self, PyObject* args)
{
    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));

    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
    backendHandler.BackendLoaded({ pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_SUCCESS });

    return PyBool_FromLong(true);
}

/**
 * Method API for the Millennium module
 * This is injected individually into each plugins Python backend, enabling them to interop with Millennium's internal API.
 */
PyMethodDef* PyGetMillenniumModule()
{
    static PyMethodDef moduleMethods[] = {
        { "ready",                 EmitReadyMessage,                METH_NOARGS,                  NULL },
        { "add_browser_css",       AddBrowserCss,                   METH_VARARGS,                 NULL },
        { "add_browser_js",        AddBrowserJs,                    METH_VARARGS,                 NULL },
        { "remove_browser_module", RemoveBrowserModule,             METH_VARARGS,                 NULL },
        { "get_user_settings",     GetUserSettings,                 METH_NOARGS,                  NULL },
        { "set_user_settings_key", SetUserSettings,                 METH_VARARGS,                 NULL },
        { "version",               GetVersionInfo,                  METH_NOARGS,                  NULL },
        { "steam_path",            GetSteamPath,                    METH_NOARGS,                  NULL },
        { "get_install_path",      GetInstallPath,                  METH_NOARGS,                  NULL },
        { "call_frontend_method",  (PyCFunction)CallFrontendMethod, METH_VARARGS | METH_KEYWORDS, NULL },
        { "is_plugin_enabled",     IsPluginEnable,                  METH_VARARGS,                 NULL },
        { NULL,                    NULL,                            0,                            NULL }  // Sentinel
    };

    return moduleMethods;
}