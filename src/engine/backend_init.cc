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

#include "millennium/plugin_loader.h"
#include <memory>
#ifdef _WIN32
#include "millennium/plat_msg.h"
#endif
#include "millennium/lua_modules.h"
#include "millennium/backend_init.h"
#include "millennium/types.h"
#include "millennium/auth.h"
#include "millennium/backend_mgr.h"
#include "millennium/environment.h"
#include "millennium/logger.h"
#include "millennium/life_cycle.h"
#include "millennium/url_parser.h"
#include "instrumentation/smem.h"
#include <future>

std::tuple<std::string, std::string> Python_GetActiveExceptionInformation();

static int plugin_loader_userdata_id;
constexpr static const char* plugin_loader_userdata_binding_name = "millennium_internal_plugin_loader_ud";

std::shared_ptr<plugin_loader> backend_initializer::get_plugin_loader_from_lua_vm(lua_State* L)
{
    lua_pushlightuserdata(L, &plugin_loader_userdata_id);
    lua_gettable(L, LUA_REGISTRYINDEX);

    auto* wp = static_cast<std::weak_ptr<plugin_loader>*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (!wp) return {};
    return wp->lock();
}

std::shared_ptr<plugin_loader> backend_initializer::get_plugin_loader_from_python_vm()
{
    PyObject* builtins = PyEval_GetBuiltins();
    PyObject* capsule = PyDict_GetItemString(builtins, plugin_loader_userdata_binding_name);
    if (!capsule) {
        return {};
    }

    auto* wp = static_cast<std::weak_ptr<plugin_loader>*>(PyCapsule_GetPointer(capsule, plugin_loader_userdata_binding_name));
    std::shared_ptr<plugin_loader> sp;
    if (wp) sp = wp->lock();

    return sp;
}

int backend_initializer::lua_set_plugin_loader_ud(lua_State* L, std::weak_ptr<plugin_loader> wp)
{
    lua_pushlightuserdata(L, &plugin_loader_userdata_id);

    void* ud = lua_newuserdata(L, sizeof(std::weak_ptr<plugin_loader>));
    new (ud) std::weak_ptr<plugin_loader>(std::move(wp));

    if (luaL_newmetatable(L, plugin_loader_userdata_binding_name)) {
        lua_pushcfunction(L, [](lua_State* L) -> int
        {
            auto* wp = static_cast<std::weak_ptr<plugin_loader>*>(lua_touserdata(L, 1));
            wp->~weak_ptr();
            return 0;
        });
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    lua_settable(L, LUA_REGISTRYINDEX);
    return plugin_loader_userdata_id;
}
void backend_initializer::python_set_plugin_loader_ud(std::weak_ptr<plugin_loader> wp)
{
    PyObject* capsule = PyCapsule_New(new std::weak_ptr<plugin_loader>(std::move(wp)), plugin_loader_userdata_binding_name, [](PyObject* cap)
    {
        auto* wp = static_cast<std::weak_ptr<plugin_loader>*>(PyCapsule_GetPointer(cap, plugin_loader_userdata_binding_name));
        if (wp) wp->~weak_ptr<plugin_loader>();
        delete wp;
    });

    PyObject* builtins = PyEval_GetBuiltins();
    PyDict_SetItemString(builtins, plugin_loader_userdata_binding_name, capsule);
    Py_DECREF(capsule);
}

/**
 * Appends custom directories to the Python `sys.path`.
 *
 * @param {std::vector<std::filesystem::path>} sitePackages - A list of custom paths to be added to the Python `sys.path`.
 *
 * This function imports the Python `sys` module and appends the provided list of directories to the `sys.path`
 * list, which Python uses to search for modules. On Windows, the function clears the existing `sys.path` to
 * prevent conflicts with installed Python versions before appending the new directories.
 *
 * Error Handling:
 * - If the `sys` module cannot be imported, an error is logged.
 * - If the `sys.path` attribute cannot be accessed, no action is taken.
 */
void backend_initializer::python_append_sys_path_modules(std::vector<std::filesystem::path> sitePackages)
{
    PyObject* sysModule = PyImport_ImportModule("sys");
    if (!sysModule) {
        LOG_ERROR("couldn't import system module");
        return;
    }

    PyObject* systemPath = PyObject_GetAttrString(sysModule, "path");

    if (systemPath) {
#ifdef _WIN32
        // Wipe the system path clean when on windows
        // - Prevents clashing installed python versions
        PyList_SetSlice(systemPath, 0, PyList_Size(systemPath), NULL);
#endif

        for (const auto& systemPathItem : sitePackages) {
            PyList_Append(systemPath, PyUnicode_FromString(systemPathItem.generic_string().c_str()));
        }
        Py_DECREF(systemPath);
    }
    Py_DECREF(sysModule);
}

/**
 * Adds a custom directory to the Python site module's path.
 *
 * @param {std::filesystem::path} customPath - The custom path to be added to the Python site directory.
 *
 * This function imports the Python `site` module and calls its `addsitedir` function to add a custom directory
 * to the Python path. If the `site` module or the `addsitedir` function cannot be found, or if an error occurs
 * during the function call, an error is logged.
 *
 * Error Handling:
 * - If the `site` module cannot be imported, the error is printed and logged.
 * - If the `addsitedir` function cannot be retrieved or called, an error is printed and logged.
 */
void backend_initializer::python_add_site_packages_directory(std::filesystem::path customPath)
{
    PyObject* siteModule = PyImport_ImportModule("site");

    if (!siteModule) {
        PyErr_Print();
        LOG_ERROR("couldn't import site module");
        return;
    }

    PyObject* addSiteDirFunc = PyObject_GetAttrString(siteModule, "addsitedir");
    if (addSiteDirFunc && PyCallable_Check(addSiteDirFunc)) {
        PyObject* args = PyTuple_Pack(1, PyUnicode_FromString(customPath.generic_string().c_str()));
        PyObject* result = PyObject_CallObject(addSiteDirFunc, args);
        Py_XDECREF(result);
        Py_XDECREF(args);
        Py_XDECREF(addSiteDirFunc);
    } else {
        PyErr_Print();
        LOG_ERROR("Failed to get addsitedir function");
    }
    Py_XDECREF(siteModule);
}

/**
 * @brief Starts the plugin backend by calling the _load method on the plugin instance.
 *
 * @param global_dict The global dictionary of the Python interpreter.
 */
void backend_initializer::python_invoke_plugin_main_fn(PyObject* global_dict, std::string pluginName)
{
    const auto PrintError = [&pluginName]()
    {
        const auto [errorMessage, traceback] = Python_GetActiveExceptionInformation();
        PyErr_Clear();

        const auto formattedMessage = fmt::format("Millennium failed to call _load on {}: {}\n{}{}", pluginName, COL_RED, traceback, COL_RESET);

        ErrorToLogger(pluginName, formattedMessage);
        logger.print(" BOOT ", formattedMessage, COL_RED);
    };

    PyObject* pluginComponent = PyDict_GetItemString(global_dict, "Plugin");

    if (!pluginComponent || !PyCallable_Check(pluginComponent)) {
        PrintError();
        return;
    }

    PyObject* pluginComponentInstance = PyObject_CallObject(pluginComponent, NULL);

    if (!pluginComponentInstance) {
        PrintError();
        return;
    }

    PyDict_SetItemString(global_dict, "plugin", pluginComponentInstance);
    PyObject* loadMethodAttribute = PyObject_GetAttrString(pluginComponentInstance, "_load");

    if (!loadMethodAttribute || !PyCallable_Check(loadMethodAttribute)) {
        PrintError();
        return;
    }

    PyObject* result = PyObject_CallObject(loadMethodAttribute, NULL);
    if (result == NULL) {
        PrintError();
    } else {
        Py_DECREF(result);
    }

    Py_DECREF(loadMethodAttribute);
    Py_DECREF(pluginComponentInstance);
}

/**
 * Sets up the plugin settings parser in the builtins dictionary.
 *
 * This function checks if the `__millennium_plugin_settings_parser__` function exists in the builtins dictionary.
 * If it does not exist, it creates a placeholder function and adds it to the builtins dictionary.
 *
 * Error Handling:
 * - If the builtins dictionary cannot be retrieved, a `RuntimeError` is raised.
 * - If creating the placeholder function fails, a `RuntimeError` is raised.
 */
void backend_initializer::compat_setup_fake_plugin_settings()
{
    PyObject* builtins = PyEval_GetBuiltins();
    if (!builtins) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve __builtins__.");
        return;
    }

    PyObject* parserFunc = PyDict_GetItemString(builtins, "__millennium_plugin_settings_parser__");
    if (!parserFunc) {
        logger.print(" BOOT ", "Creating __millennium_plugin_settings_parser__ function in builtins.", COL_YELLOW);

        static PyMethodDef methodDef = { "__millennium_plugin_settings_parser__", [](PyObject*, PyObject*) -> PyObject* { Py_RETURN_FALSE; }, METH_NOARGS,
                                         "Millennium plugin settings parser placeholder." };

        PyObject* newFunc = PyCFunction_New(&methodDef, nullptr);

        if (newFunc) {
            PyDict_SetItemString(builtins, "__millennium_plugin_settings_parser__", newFunc);
            Py_DECREF(newFunc);
        } else {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create __millennium_plugin_settings_parser__ function.");
        }
    }
}

/**
 * Sets a secret plugin name in both the global dictionary and the builtins dictionary in Python.
 * The plugin secret name is used to identify the plugin in IPC and FFI calls.
 *
 * @param {PyObject*} globalDictionary - The global dictionary where the secret name is stored.
 * @param {std::string} pluginName - The name of the plugin to set as the secret name.
 *
 * This function sets the `MILLENNIUM_PLUGIN_SECRET_NAME` variable in both:
 * 1. The provided `globalDictionary`.
 * 2. The Python `__builtins__` dictionary.
 *
 * Error Handling:
 * - If the `__builtins__` dictionary cannot be retrieved, a `RuntimeError` is raised.
 * - If setting the `MILLENNIUM_PLUGIN_SECRET_NAME` in `__builtins__` fails, a `RuntimeError` is raised.
 */
void backend_initializer::set_plugin_internal_name(PyObject* globalDictionary, const std::string& pluginName)
{
    /** Set the secret name in the global dictionary, i.e the global scope */
    PyDict_SetItemString(globalDictionary, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(pluginName.c_str()));

    PyObject* builtins = PyEval_GetBuiltins();
    if (!builtins) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve __builtins__.");
        return;
    }

    // Set the variable in the __builtins__ dictionary
    if (PyDict_SetItemString(builtins, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(pluginName.c_str())) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set the variable in __builtins__.");
    }
}

/**
 * Sets environment variables for a plugin in the global dictionary.
 * __file__ is not initially set by Python, so we set it here.
 *
 * @param {PyObject*} globalDictionary - The global dictionary where the environment variables are set.
 * @param {SettingsStore::PluginTypeSchema} plugin - The plugin information containing the base directory and backend directory.
 *
 * This function sets the following environment variables in the provided `globalDictionary`:
 * 1. `PLUGIN_BASE_DIR` - The base directory of the plugin.
 * 2. `__file__` - The path to the `main.py` file in the plugin's backend directory.
 *
 * Both paths are converted to strings and set as Python variables in the global dictionary.
 */
void backend_initializer::set_plugin_environment_variables(PyObject* globalDictionary, const plugin_manager::plugin_t& plugin)
{
    PyDict_SetItemString(globalDictionary, "PLUGIN_BASE_DIR", PyUnicode_FromString(plugin.plugin_base_dir.generic_string().c_str()));
    PyDict_SetItemString(globalDictionary, "__file__", PyUnicode_FromString((plugin.plugin_backend_dir / "main.py").generic_string().c_str()));
}

static void RegisterModule(lua_State* L, const char* name, lua_CFunction func)
{
    lua_pushcclosure(L, func, 0);
    lua_setfield(L, -2, name);
}

void backend_initializer::lua_backend_started_cb(plugin_manager::plugin_t plugin, const std::weak_ptr<plugin_loader> weak_plugin_loader, lua_State* L)
{
    logger.log("Starting Lua backend for '{}'", plugin.plugin_name);

    if (m_backend_manager->lua_is_locked(L)) {
        LOG_ERROR("Lua mutex is already locked for plugin '{}'", plugin.plugin_name);
        return;
    }

    m_backend_manager->lua_lock(L);

    lua_pushstring(L, plugin.plugin_name.c_str());
    lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");

    lua_pushstring(L, plugin.plugin_backend_dir.parent_path().string().c_str());
    lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_BACKEND_ABSOLUTE");

    lua_set_plugin_loader_ud(L, weak_plugin_loader);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string currentPath = lua_tostring(L, -1);
    std::string pluginPath = plugin.plugin_backend_dir.parent_path().string();
    std::string newPath = pluginPath + "/?.lua;" + currentPath;
    lua_pop(L, 1);
    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    RegisterModule(L, "json", luaopen_cjson);
    RegisterModule(L, "millennium", luaopen_millennium_lib);
    RegisterModule(L, "http", luaopen_http_lib);
    RegisterModule(L, "utils", luaopen_utils_lib);
    RegisterModule(L, "logger", luaopen_logger_lib);
    RegisterModule(L, "fs", luaopen_fs_lib);
    RegisterModule(L, "regex", luaopen_regex_lib);
    RegisterModule(L, "datetime", luaopen_datatime_lib);

    lua_pop(L, 2);

    if (luaL_dofile(L, (plugin.plugin_backend_dir).string().c_str()) != LUA_OK) {
        LOG_ERROR("Lua error: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        m_backend_manager->lua_unlock(L);
        return;
    }

    lua_pushvalue(L, -1);
    lua_setglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");

    /** parse the "patches" block */
    load_patches(L, g_lb_patch_arena);
    hashmap_print(g_lb_patch_arena);

    if (!lua_istable(L, -1)) {
        LOG_ERROR("Lua file should return a table with functions");
        lua_pop(L, 1);
        lua_close(L);
        m_backend_manager->lua_unlock(L);
        return;
    }

    lua_getfield(L, -1, "on_load");
    if (!lua_isfunction(L, -1)) {
        LOG_ERROR("Failed to locate 'on_load' function in plugin backend for '{}'", plugin.plugin_name);
        lua_pop(L, 2);
        lua_close(L);
        m_backend_manager->lua_unlock(L);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        LOG_ERROR("Error calling on_load in plugin backend for '{}': {}", plugin.plugin_name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    m_backend_manager->lua_unlock(L);
}

/**
 * Callback function to start the backend for a given plugin, setting up the necessary environment
 * and executing the plugin's main module.
 *
 * @param {SettingsStore::PluginTypeSchema} plugin - The plugin configuration containing the plugin name, directories, and other settings.
 *
 * This function performs the following tasks:
 * 1. Sets the plugin's secret name and environment variables in the Python global dictionary.
 * 2. Modifies the Python `sys.path` to include directories related to the plugin and its dependencies.
 * 3. For Windows, adjusts the virtual PATH and adds required directories for Python binaries and libraries.
 * 4. Appends the plugin's virtual environment and site-packages directory to `sys.path`.
 * 5. Attempts to open and execute the main module of the plugin. If the file cannot be opened or there is an error during execution, it logs the error and notifies the backend
 * handler.
 * 6. If successful, it calls `StartPluginBackend` to continue the plugin startup process.
 *
 * Error Handling:
 * - If any step of the process fails (e.g., file opening, module import), the error is logged and the backend load is marked as failed.
 */
void backend_initializer::python_backend_started_cb(plugin_manager::plugin_t plugin, const std::weak_ptr<plugin_loader> weak_plugin_loader)
{
    const auto [pythonPath, pythonLibs, pythonUserLibs] = GetPythonEnvPaths();

    PyObject* globalDictionary = PyModule_GetDict(PyImport_AddModule("__main__"));
    const auto backendMainModule = plugin.plugin_backend_dir.generic_string();

    // associate the plugin name with the running plugin. used for IPC/FFI
    this->set_plugin_internal_name(globalDictionary, plugin.plugin_name);
    this->set_plugin_environment_variables(globalDictionary, plugin);
    this->python_set_plugin_loader_ud(weak_plugin_loader);

    std::vector<std::filesystem::path> sysPath;
    sysPath.push_back(plugin.plugin_base_dir / plugin.plugin_backend_dir.parent_path());

#ifdef _WIN32
    {
        std::wstring pythonPathW = std::wstring(pythonPath.begin(), pythonPath.end());
        /* Add local python binaries to virtual PATH to prevent changing actual PATH */
        AddDllDirectory(pythonPathW.c_str());

        sysPath.push_back(pythonPath);
        sysPath.push_back(pythonLibs);
    }
#endif

    python_append_sys_path_modules(sysPath);
    python_add_site_packages_directory(pythonUserLibs);

    PyObject* mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
    FILE* mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r");

    if (mainModuleFilePtr == NULL) {
        logger.warn("failed to fopen file @ {}", backendMainModule);
        ErrorToLogger(plugin.plugin_name, fmt::format("Failed to open file @ {}", backendMainModule));

        m_backend_event_dispatcher->backend_loaded_event_hdlr({ plugin.plugin_name, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_FAILED });
        return;
    }

    PyObject* mainModule = PyImport_AddModule("__main__");
    PyObject* mainModuleDict = PyModule_GetDict(mainModule);

    if (!mainModule || !mainModuleDict) {
        logger.warn("Millennium failed to initialize the main module.");
        ErrorToLogger(plugin.plugin_name, "Failed to initialize the main module.");

        m_backend_event_dispatcher->backend_loaded_event_hdlr({ plugin.plugin_name, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_FAILED });
        fclose(mainModuleFilePtr);
        return;
    }

    logger.log("Running plugin: {}", plugin.plugin_name);

    PyObject* result = PyRun_FileEx(mainModuleFilePtr, backendMainModule.c_str(), Py_file_input, mainModuleDict, mainModuleDict, 1);

    if (!result) {
        const auto [errorMessage, traceback] = Python_GetActiveExceptionInformation();

        logger.print(" PY-MAN ", fmt::format("Millennium failed to start {}: {}\n{}{}", plugin.plugin_name, COL_RED, traceback, COL_RESET), COL_RED);
        logger.warn("Millennium failed to start '{}'. This is likely due to failing module side effects, unrelated to Millennium.", plugin.plugin_name);

        ErrorToLogger(plugin.plugin_name,
                      fmt::format("Failed to start plugin: {}. This is likely due to failing module side effects, unrelated to Millennium.\n\n{}", plugin.plugin_name, traceback));

        m_backend_event_dispatcher->backend_loaded_event_hdlr({ plugin.plugin_name, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_FAILED });
        return;
    }

    Py_DECREF(result);

    const auto startTime = std::chrono::steady_clock::now();
    std::atomic<bool> timeOutLockThreadRunning = true;

    std::thread timeOutThread([dispatcher = m_backend_event_dispatcher, settings = m_settings_store, &timeOutLockThreadRunning, startTime, plugin]
    {
        while (timeOutLockThreadRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30)) {
                std::string errorMessage = fmt::format(
                    "\nIt appears that the plugin '{}' either forgot to call `Millennium.ready()` or is I/O blocking the main thread. We've flagged it as a failure to load."
                    "Your _load() function MUST NOT block the main thread, logic that runs for the duration of the plugin should run in true parallelism with threading.",
                    plugin.plugin_name);

                LOG_ERROR(errorMessage);
                ErrorToLogger(plugin.plugin_name, errorMessage);

#ifdef _WIN32
                const int result =
                    Plat_ShowMessageBox("Millennium - Startup Error",
                                        fmt::format("It appears that the plugin '{}' has either crashed or is taking too long to respond, this may cause side effects "
                                                    "or break the Steam UI. Would you like to disable it on next Steam startup?",
                                                    plugin.plugin_name)
                                            .c_str(),
                                        MESSAGEBOX_QUESTION);

                if (result == IDYES) {
                    settings->TogglePluginStatus(plugin.plugin_name, false);
                }
#endif
                dispatcher->backend_loaded_event_hdlr({ plugin.plugin_name, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_FAILED });
                break;
            }
        }
    });

    compat_setup_fake_plugin_settings();
    python_invoke_plugin_main_fn(globalDictionary, plugin.plugin_name);

    timeOutLockThreadRunning.store(false);
    timeOutThread.join();
}

/**
 * Restores the original `SharedJSContext` by renaming the backup file to the original file.
 * It reverses the patches done in the preloader module
 *
 * @note this function is only applicable to Windows
 */
void backend_initializer::compat_restore_shared_js_context()
{
#ifdef _WIN32
    logger.log("Restoring SharedJSContext...");

    const auto SteamUIModulePath = platform::GetSteamPath() / "steamui" / "index.html";

    /** if the sequence isn't found, it indicates it hasn't been patched by millennium <= 2.30.0 preloader */
    if (platform::ReadFileSync(SteamUIModulePath.string()).find("<!doctype html><html><head><title>SharedJSContext</title></head></html>") == std::string::npos) {
        logger.log("SharedJSContext isn't patched, skipping...");
        return;
    }

    const auto librariesPath = platform::GetSteamPath() / "steamui" / "libraries";
    std::string libraryChunkJS;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(librariesPath)) {
            if (entry.is_regular_file() && entry.path().filename().string().substr(0, 10) == "libraries~" && entry.path().extension() == ".js") {
                libraryChunkJS = entry.path().filename().string();
                break;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        logger.warn("Failed to find libraries~xxx.js: {}", e.what());
    }

    if (libraryChunkJS.empty()) {
        MessageBoxA(NULL,
                    "Millennium failed to find a key library used by Steam. "
                    "Let our developers know if you see this message, it's likely a bug.\n"
                    "You can reach us over at steambrew.app/discord",
                    "Millennium", MB_ICONERROR);

        return;
    }

    std::string fileContent = fmt::format(
        R"(<!doctype html><html style="width: 100%; height: 100%"><head><title>SharedJSContext</title><meta charset="utf-8"><script defer="defer" src="/libraries/{}"></script><script defer="defer" src="/library.js"></script><link href="/css/library.css" rel="stylesheet"></head><body style="width: 100%; height: 100%; margin: 0; overflow: hidden;"><div id="root" style="height:100%; width: 100%"></div><div style="display:none"></div></body></html>)",
        libraryChunkJS);

    try {
        platform::WriteFileSync(SteamUIModulePath.string(), fileContent);
    } catch (const std::system_error& e) {
        logger.warn("Failed to restore SharedJSContext: {}", e.what());
    } catch (const std::exception& e) {
        logger.warn("Failed to restore SharedJSContext: {}", e.what());
    }

    logger.log("Restored SharedJSContext...");
#endif
}

void backend_initializer::start_package_manager(std::weak_ptr<plugin_loader> plugin_loader)
{
    std::promise<void> promise;

    plugin_manager::plugin_t plugin;
    plugin.plugin_name = "pipx";
#ifdef MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS
    plugin.plugin_backend_dir = std::filesystem::path(platform::environment::get("MILLENNIUM__ASSETS_PATH")) / "package_manager";
#else
    plugin.plugin_backend_dir = std::filesystem::path(platform::environment::get("MILLENNIUM__ASSETS_PATH")) / "pipx";
#endif
    plugin.is_internal = true;

    /** Create instance on a separate thread to prevent IO blocking of concurrent
     * threads */
    m_backend_manager->create_python_vm(plugin, [this, plugin_loader, &promise](plugin_manager::plugin_t plugin)
    {
        logger.log("Started preloader module");
        const auto backendMainModule = (plugin.plugin_backend_dir / "main.py").generic_string();

        PyObject* globalDictionary = PyModule_GetDict(PyImport_AddModule("__main__"));
        /** Set plugin name in the global dictionary so its stdout can be
         * retrieved by the logger. */
        set_plugin_internal_name(globalDictionary, plugin.plugin_name);
        python_set_plugin_loader_ud(plugin_loader);

        PyObject* mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
        FILE* mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r");

        if (mainModuleFilePtr == NULL) {
            LOG_ERROR("Failed to fopen file @ {}", backendMainModule);
            ErrorToLogger(plugin.plugin_name, fmt::format("Failed to open file @ {}", backendMainModule));
            return;
        }

        try {
            logger.log("Starting package manager thread @ {}", backendMainModule);

            if (PyRun_SimpleFile(mainModuleFilePtr, backendMainModule.c_str()) != 0) {
                LOG_ERROR("Failed to run PIPX preload", plugin.plugin_name);
                ErrorToLogger(plugin.plugin_name, "Failed to preload plugins");
                return;
            }
        } catch (const std::system_error& error) {
            LOG_ERROR("Failed to run PIPX preload due to a system error: {}", error.what());
        }

        logger.log("Preloader finished...");
        promise.set_value();
    });

    /* Wait for the package manager plugin to exit, signalling we can now start
     * other plugins */
    promise.get_future().get();
    m_backend_manager->python_destroy_vm("pipx");
}
