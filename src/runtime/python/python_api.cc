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

#include "millennium/config.h"
#include "millennium/life_cycle.h"
#include "millennium/core_ipc.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/filesystem.h"
#include "millennium/plugin_loader.h"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

PyObject* GetUserSettings([[maybe_unused]] PyObject* self, [[maybe_unused]] PyObject* args)
{
    PyErr_SetString(PyExc_NotImplementedError, "get_user_settings is not implemented yet. It will likely be removed in the future.");
    return NULL;
}

PyObject* SetUserSettings([[maybe_unused]] PyObject* self, [[maybe_unused]] PyObject* args)
{
    PyErr_SetString(PyExc_NotImplementedError, "set_user_settings_key is not implemented yet. It will likely be removed in the future.");
    return NULL;
}

PyObject* CallFrontendMethod([[maybe_unused]] PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* methodName = NULL;
    PyObject* parameterList = NULL;

    static const char* keywordArgsList[] = { "method_name", "params", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", (char**)keywordArgsList, &methodName, &parameterList)) {
        return NULL;
    }

    std::vector<ipc_main::javascript_parameter> params;

    if (parameterList != NULL) {
        if (!PyList_Check(parameterList)) {
            PyErr_SetString(PyExc_TypeError, "params must be a list");
            return NULL;
        }

        Py_ssize_t listSize = PyList_Size(parameterList);

        for (Py_ssize_t i = 0; i < listSize; ++i) {
            PyObject* item = PyList_GetItem(parameterList, i);

            if (PyBool_Check(item)) {
                params.emplace_back(static_cast<bool>(PyObject_IsTrue(item)));
            } else if (PyLong_Check(item)) {
                int overflow = 0;
                long long v = PyLong_AsLongLongAndOverflow(item, &overflow);

                if (overflow != 0) {
                    PyErr_SetString(PyExc_OverflowError, "integer too large for int64");
                    return NULL;
                }

                params.emplace_back(static_cast<int64_t>(v));
            } else if (PyFloat_Check(item)) {
                params.emplace_back(PyFloat_AsDouble(item));
            } else if (PyUnicode_Check(item)) {
                params.emplace_back(std::string(PyUnicode_AsUTF8(item)));
            } else {
                PyErr_SetString(PyExc_TypeError, "Millennium IPC only supports [bool, int, float, str]");
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

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_python_vm();
    if (!loader) {
        LOG_ERROR("Failed to contact Millennium's plugin loader, it's likely shut down.");
        return NULL;
    }

    std::shared_ptr<ipc_main> ipc = loader->get_ipc_main();
    if (!ipc) {
        LOG_ERROR("Failed to contact Millennium's IPC, it's likely shut down.");
        return NULL;
    }

    const std::string script = ipc->compile_javascript_expression(pluginName, methodName, params);
    return ipc->evaluate_javascript_expression(script).to_python();
}

PyObject* GetVersionInfo([[maybe_unused]] PyObject* self, [[maybe_unused]] PyObject* args)
{
    return PyUnicode_FromString(MILLENNIUM_VERSION);
}

PyObject* GetSteamPath([[maybe_unused]] PyObject* self, [[maybe_unused]] PyObject* args)
{
    return PyUnicode_FromString(platform::get_steam_path().string().c_str());
}

PyObject* GetInstallPath([[maybe_unused]] PyObject* self, [[maybe_unused]] PyObject* args)
{
    return PyUnicode_FromString(platform::get_install_path().string().c_str());
}

PyObject* RemoveBrowserModule([[maybe_unused]] PyObject* self, PyObject* args)
{
    int moduleId;

    if (!PyArg_ParseTuple(args, "i", &moduleId)) {
        return PyBool_FromLong(false);
    }

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_python_vm();
    if (!loader) {
        LOG_ERROR("Failed to contact Millennium's plugin loader, it's likely shut down.");
        return PyBool_FromLong(false);
    }

    std::shared_ptr<millennium_backend> backend = loader->get_millennium_backend();
    if (!backend) {
        LOG_ERROR("Failed to contact Millennium's backend, it's likely shut down.");
        return PyBool_FromLong(false);
    }

    std::weak_ptr<theme_webkit_mgr> webkit_mgr_weak = backend->get_theme_webkit_mgr();
    if (auto webkit_mgr = webkit_mgr_weak.lock()) {
        return PyBool_FromLong(webkit_mgr->remove_browser_hook(static_cast<unsigned long long>(moduleId)));
    }

    LOG_ERROR("Failed to lock theme_webkit_mgr, it likely shutdown.");
    return PyBool_FromLong(false);
}

unsigned long long AddBrowserModule(PyObject* args, network_hook_ctl::TagTypes type)
{
    const char* moduleItem;
    const char* regexSelector = ".*";

    if (!PyArg_ParseTuple(args, "s|s", &moduleItem, &regexSelector)) {
        return 0;
    }

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_python_vm();
    if (!loader) {
        LOG_ERROR("Failed to contact Millennium's plugin loader, it's likely shut down.");
        return -1;
    }

    std::shared_ptr<millennium_backend> backend = loader->get_millennium_backend();
    if (!backend) {
        LOG_ERROR("Failed to contact Millennium's backend, it's likely shut down.");
        return -1;
    }

    std::weak_ptr<theme_webkit_mgr> webkit_mgr_weak = backend->get_theme_webkit_mgr();
    if (auto webkit_mgr = webkit_mgr_weak.lock()) {
        return webkit_mgr->add_browser_hook(moduleItem, regexSelector, type);
    }

    LOG_ERROR("Failed to lock theme_webkit_mgr, it likely shutdown.");
    return -1;
}

PyObject* AddBrowserCss([[maybe_unused]] PyObject* self, PyObject* args)
{
    return PyLong_FromLong((long)AddBrowserModule(args, network_hook_ctl::TagTypes::STYLESHEET));
}

PyObject* AddBrowserJs([[maybe_unused]] PyObject* self, PyObject* args)
{
    return PyLong_FromLong((long)AddBrowserModule(args, network_hook_ctl::TagTypes::JAVASCRIPT));
}

/**
 * A utility function to check if a plugin is enabled.
 */
PyObject* IsPluginEnable([[maybe_unused]] PyObject* self, PyObject* args)
{
    const char* pluginName = NULL;

    if (!PyArg_ParseTuple(args, "s", &pluginName)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to parse parameters");
        return NULL;
    }

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_python_vm();
    if (!loader) {
        LOG_ERROR("Failed to contact Millennium's plugin loader, it's likely shut down.");
        return NULL;
    }

    std::shared_ptr<settings_store> settings = loader->get_settings_store();
    if (!settings) {
        LOG_ERROR("Failed to contact Millennium's settings store, it's likely shut down.");
        return NULL;
    }

    bool isEnabled = settings->is_enabled(pluginName);
    return PyBool_FromLong(isEnabled);
}

PyObject* EmitReadyMessage([[maybe_unused]] PyObject* self, [[maybe_unused]] PyObject* args)
{
    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* pluginNameObj = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (pluginNameObj == nullptr || PyErr_Occurred()) {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    const std::string pluginName = PyUnicode_AsUTF8(PyObject_Str(pluginNameObj));

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_python_vm();
    if (!loader) {
        LOG_ERROR("Failed to contact Millennium's plugin loader, it's likely shut down.");
        return NULL;
    }

    std::shared_ptr<backend_event_dispatcher> backend_dispatcher = loader->get_backend_event_dispatcher();
    if (!backend_dispatcher) {
        LOG_ERROR("Failed to contact Millennium's backend event dispatcher, it's likely shut down.");
        return NULL;
    }

    backend_dispatcher->backend_loaded_event_hdlr({ pluginName, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_SUCCESS });
    return PyBool_FromLong(true);
}

/**
 * Method API for the Millennium module
 * This is injected individually into each plugins Python backend, enabling them to interop with Millennium's internal API.
 */
PyMethodDef* PyGetMillenniumModule()
{
#ifdef __linux__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
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
        /** this is 100% a valid cast, shut up gcc */
        { "call_frontend_method",  (PyCFunction)CallFrontendMethod, METH_VARARGS | METH_KEYWORDS, NULL },
        { "is_plugin_enabled",     IsPluginEnable,                  METH_VARARGS,                 NULL },
        { NULL,                    NULL,                            0,                            NULL }  // Sentinel
    };
#ifdef __linux__
#pragma GCC diagnostic pop
#endif

    return moduleMethods;
}
