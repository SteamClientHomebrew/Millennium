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

#include "co_stub.h"
#include <vector>
#include <fmt/core.h>
#include "co_spawn.h"
#include "internal_logger.h"
#include "loader.h"
#include "http_hooks.h"
#include "ffi.h"
#include <tuple>
#include "plugin_logger.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include "encoding.h"
#include "url_parser.h"
#include <env.h>

#include <secure_socket.h>

static std::string addedScriptOnNewDocumentId = "";

/**
 * @brief A singleton class that manages the state of the backend load.
 * 
 * This class is used to manage the state of the backend load.
 * 
 * @returns {BackendLoadState} - A singleton instance of the BackendLoadState class.
 */
class BackendLoadState {
private:
    BackendLoadState() = default;
    static BackendLoadState& getInstance() {
        static BackendLoadState instance;
        return instance;
    }

public:
    std::mutex mtx;
    std::condition_variable cvScript;
    bool hasScriptIdentifier = false;

    static BackendLoadState& get() {
        return getInstance();
    }
};

/**
 * Constructs a JavaScript bootstrap module that includes the given script modules and a port number.
 *
 * @param {std::vector<std::string>} scriptModules - A list of script module names to include in the bootstrap.
 * @param {uint16_t} port - The port number to be included in the bootstrap call.
 * @returns {std::string} - The constructed JavaScript module as a string.
 * 
 * This function reads a plugin helper module part of my plugutil repository, `client_api.js`,
 * and appends the provided script modules to it. The port number is also included in the final script.
 * The final script is returned as a string, ready for use.
 *
 * Error Handling:
 * - If the `client_api.js` file cannot be read, an error is logged, and a message box is shown on Windows.
 */
const std::string GetBootstrapModule(const std::vector<std::string> scriptModules)
{
    std::string scriptModuleArray;
    std::optional<std::string> millenniumPreloadPath = SystemIO::GetMillenniumPreloadPath();

    if (!millenniumPreloadPath.has_value())
    {
        LOG_ERROR("Missing client preload module. Please re-install Millennium.");
        #ifdef _WIN32
        MessageBoxA(NULL, "Missing client preload module. Please re-install Millennium.", "Millennium", MB_ICONERROR);
        #endif
    }

    for (int i = 0; i < scriptModules.size(); i++)
    {
        scriptModuleArray.append(fmt::format("\"{}\"{}", scriptModules[i], (i == scriptModules.size() - 1 ? "" : ",")));
    }

    const std::string millenniumAuthToken = GetAuthToken();

    const std::string ftpPath = UrlFromPath("https://millennium.ftp/", millenniumPreloadPath.value_or(std::string()));
    const std::string scriptContent = fmt::format("(new module.default).StartPreloader('{}', [{}]);", millenniumAuthToken, scriptModuleArray);

    return fmt::format("import('{}').then(module => {{ {} }})", ftpPath, scriptContent);
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
const void AppendSysPathModules(std::vector<std::filesystem::path> sitePackages) 
{
    PyObject *sysModule = PyImport_ImportModule("sys");
    if (!sysModule) 
    {
        LOG_ERROR("couldn't import system module");
        return;
    }

    PyObject *systemPath = PyObject_GetAttrString(sysModule, "path");

    if (systemPath) 
    {
#ifdef _WIN32
        // Wipe the system path clean when on windows
        // - Prevents clashing installed python versions
        PyList_SetSlice(systemPath, 0, PyList_Size(systemPath), NULL);
#endif

        for (const auto& systemPathItem : sitePackages) 
        {
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
void AddSitePackagesDirectory(std::filesystem::path customPath)
{
    PyObject *siteModule = PyImport_ImportModule("site");

    if (!siteModule) 
    {
        PyErr_Print();
        LOG_ERROR("couldn't import site module");
        return;
    }

    PyObject *addSiteDirFunc = PyObject_GetAttrString(siteModule, "addsitedir");
    if (addSiteDirFunc && PyCallable_Check(addSiteDirFunc)) 
    {
        PyObject *args = PyTuple_Pack(1, PyUnicode_FromString(customPath.generic_string().c_str()));
        PyObject *result = PyObject_CallObject(addSiteDirFunc, args);
        Py_XDECREF(result);
        Py_XDECREF(args);
        Py_XDECREF(addSiteDirFunc);
    } 
    else 
    {
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
void StartPluginBackend(PyObject* global_dict, std::string pluginName) 
{
    const auto PrintError = [&pluginName]() 
    {
        const auto [errorMessage, traceback] = Python::ActiveExceptionInformation();
        PyErr_Clear();

        const auto formattedMessage = fmt::format("Millennium failed to call _load on {}: {}\n{}{}", pluginName, COL_RED, traceback, COL_RESET);

        ErrorToLogger(pluginName, formattedMessage);
        Logger.PrintMessage(" BOOT ", formattedMessage, COL_RED);
    };

    PyObject *pluginComponent = PyDict_GetItemString(global_dict, "Plugin");

    if (!pluginComponent || !PyCallable_Check(pluginComponent)) 
    {
        PrintError();
        return;
    }

    PyObject *pluginComponentInstance = PyObject_CallObject(pluginComponent, NULL);

    if (!pluginComponentInstance) 
    {
        PrintError();
        return;
    }

    PyDict_SetItemString(global_dict, "plugin", pluginComponentInstance);
    PyObject *loadMethodAttribute = PyObject_GetAttrString(pluginComponentInstance, "_load");

    if (!loadMethodAttribute || !PyCallable_Check(loadMethodAttribute)) 
    {
        PrintError();
        return;
    }

    PyObject *result = PyObject_CallObject(loadMethodAttribute, NULL);
    if (result == NULL) 
    {
        PrintError();
    } 
    else 
    {
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
void SetupPluginSettings()
{
    PyObject* builtins = PyEval_GetBuiltins();
    if (!builtins) 
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve __builtins__.");
        return;
    }

    PyObject* parserFunc = PyDict_GetItemString(builtins, "__millennium_plugin_settings_parser__");
    if (!parserFunc) 
    {
        Logger.PrintMessage(" BOOT ", "Creating __millennium_plugin_settings_parser__ function in builtins.", COL_YELLOW);

        static PyMethodDef methodDef = {
            "__millennium_plugin_settings_parser__",
            [](PyObject*, PyObject*) -> PyObject* { Py_RETURN_FALSE; },
            METH_NOARGS,
            "Millennium plugin settings parser placeholder."
        };

        PyObject* newFunc = PyCFunction_New(&methodDef, nullptr);

        if (newFunc) 
        {
            PyDict_SetItemString(builtins, "__millennium_plugin_settings_parser__", newFunc);
            Py_DECREF(newFunc);
        } 
        else 
        {
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
const void SetPluginSecretName(PyObject* globalDictionary, const std::string& pluginName) 
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
const void SetPluginEnvironmentVariables(PyObject* globalDictionary, const SettingsStore::PluginTypeSchema& plugin) 
{
    PyDict_SetItemString(globalDictionary, "PLUGIN_BASE_DIR", PyUnicode_FromString(plugin.pluginBaseDirectory.generic_string().c_str()));
    PyDict_SetItemString(globalDictionary, "__file__", PyUnicode_FromString((plugin.backendAbsoluteDirectory / "main.py").generic_string().c_str()));
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
 * 5. Attempts to open and execute the main module of the plugin. If the file cannot be opened or there is an error during execution, it logs the error and notifies the backend handler.
 * 6. If successful, it calls `StartPluginBackend` to continue the plugin startup process.
 *
 * Error Handling:
 * - If any step of the process fails (e.g., file opening, module import), the error is logged and the backend load is marked as failed.
 */
const void CoInitializer::BackendStartCallback(SettingsStore::PluginTypeSchema plugin) 
{
    PyObject* globalDictionary = PyModule_GetDict(PyImport_AddModule("__main__"));
    const auto backendMainModule = plugin.backendAbsoluteDirectory.generic_string();

    // associate the plugin name with the running plugin. used for IPC/FFI
    SetPluginSecretName(globalDictionary, plugin.pluginName);
    SetPluginEnvironmentVariables(globalDictionary, plugin);

    std::vector<std::filesystem::path> sysPath;
    sysPath.push_back(plugin.pluginBaseDirectory / plugin.backendAbsoluteDirectory.parent_path());

    #ifdef _WIN32
    {
        /* Add local python binaries to virtual PATH to prevent changing actual PATH */
        AddDllDirectory(pythonModulesBaseDir.wstring().c_str());

        sysPath.push_back(pythonPath);
        sysPath.push_back(pythonLibs);
    }
    #endif

    AppendSysPathModules(sysPath);
    AddSitePackagesDirectory(pythonUserLibs);

    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();

    PyObject *mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
    FILE *mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r");

    if (mainModuleFilePtr == NULL) 
    {
        Logger.Warn("failed to fopen file @ {}", backendMainModule);
        ErrorToLogger(plugin.pluginName, fmt::format("Failed to open file @ {}", backendMainModule));

        backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });
        return;
    }

    PyObject* mainModule     = PyImport_AddModule("__main__");
    PyObject* mainModuleDict = PyModule_GetDict(mainModule);

    if (!mainModule || !mainModuleDict) 
    {
        Logger.Warn("Millennium failed to initialize the main module.");
        ErrorToLogger(plugin.pluginName, "Failed to initialize the main module.");

        backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });
        fclose(mainModuleFilePtr);
        return;
    }

    Logger.Log("Running plugin: {}", plugin.pluginName);

    PyObject* result = PyRun_FileEx(mainModuleFilePtr, backendMainModule.c_str(), Py_file_input, mainModuleDict, mainModuleDict, 1);

    if (!result) 
    {
        const auto [errorMessage, traceback] = Python::ActiveExceptionInformation();

        Logger.PrintMessage(" PY-MAN ", fmt::format("Millennium failed to start {}: {}\n{}{}", plugin.pluginName, COL_RED, traceback, COL_RESET), COL_RED);
        Logger.Warn("Millennium failed to start '{}'. This is likely due to failing module side effects, unrelated to Millennium.", plugin.pluginName);

        ErrorToLogger(plugin.pluginName, fmt::format("Failed to start plugin: {}. This is likely due to failing module side effects, unrelated to Millennium.\n\n{}", plugin.pluginName, traceback));

        backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });
        return;
    }

    Py_DECREF(result);

    const auto startTime = std::chrono::steady_clock::now();
    std::atomic<bool> timeOutLockThreadRunning = true;

    std::thread timeOutThread([&timeOutLockThreadRunning, startTime, plugin] 
    {
        while (timeOutLockThreadRunning.load()) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30)) 
            {
                std::string errorMessage = fmt::format(
                    "\nIt appears that the plugin '{}' either forgot to call `Millennium.ready()` or is I/O blocking the main thread. We've flagged it as a failure to load."
                    "Your _load() function MUST NOT block the main thread, logic that runs for the duration of the plugin should run in true parallelism with threading.", plugin.pluginName
                );

                LOG_ERROR(errorMessage);
                ErrorToLogger(plugin.pluginName, errorMessage);

                #ifdef _WIN32
                const int result = MessageBoxA(
                    NULL, 
                    fmt::format("It appears that the plugin '{}' has either crashed or is taking too long to respond, this may cause side effects or break the Steam UI. Would you like to disable it on next Steam startup?", plugin.pluginName).c_str(), 
                    "Millennium - Startup Error", 
                    MB_ICONERROR | MB_YESNO
                );

                if (result == IDYES) 
                {
                    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
                    settingsStore->TogglePluginStatus(plugin.pluginName, false);
                }
                #endif

                CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
                backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });

                break;
            }
        }
    });

    SetupPluginSettings();
    StartPluginBackend(globalDictionary, plugin.pluginName);  

    timeOutLockThreadRunning.store(false);
    timeOutThread.join();  
}

/**
 * Constructs a module for loading plugins based on the given FTP and IPC ports.
 *
 * @returns {std::string} - A string representing the constructed module containing the list of plugin URLs and bootstrap configuration.
 *
 * This function performs the following tasks:
 * 1. Loads the plugin configurations using `SettingsStore`.
 * 2. Filters the enabled plugins and constructs URLs for their frontend resources using the FTP port.
 * 3. Collects the constructed URLs into a script import table.
 * 4. Calls `GetBootstrapModule` with the script import table and IPC port to return the complete module configuration.
 *
 * The constructed module is returned as a string.
 */
const std::string ConstructOnLoadModule() 
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    std::vector<SettingsStore::PluginTypeSchema> plugins = settingsStore->ParseAllPlugins();

    std::vector<std::string> scriptImportTable;
    
    for (auto& plugin : plugins)  
    {
        if (!settingsStore->IsEnabledPlugin(plugin.pluginName)) 
        {    
            continue;
        }

        const auto frontEndAbs = plugin.frontendAbsoluteDirectory.generic_string();
        scriptImportTable.push_back(UrlFromPath("https://millennium.ftp/", frontEndAbs));
    }

    return GetBootstrapModule(scriptImportTable);
}

/**
 * Restores the original `SharedJSContext` by renaming the backup file to the original file.
 * It reverses the patches done in the preloader module
 * 
 * @note this function is only applicable to Windows
 */
const void UnPatchSharedJSContext()
{
    #ifdef _WIN32
    Logger.Log("Restoring SharedJSContext...");

    const auto SteamUIModulePath       = SystemIO::GetSteamPath() / "steamui" / "index.html";
    const auto SteamUIModulePathBackup = SystemIO::GetSteamPath() / "steamui" / "orig.html";

    const auto librariesPath = SystemIO::GetSteamPath() / "steamui" / "libraries";
    std::string libraryChunkJS;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(librariesPath))
        {
            if (entry.is_regular_file() && entry.path().filename().string().substr(0, 10) == "libraries~" && entry.path().extension() == ".js")
            {
                libraryChunkJS = entry.path().filename().string();
                break;
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        Logger.Warn("Failed to find libraries~xxx.js: {}", e.what());
    }

    if (libraryChunkJS.empty())
    {
        MessageBoxA(
            NULL, 
            "Millennium failed to find a key library used by Steam. "
            "Let our developers know if you see this message, it's likely a bug.\n"
            "You can reach us over at steambrew.app/discord", 
            "Millennium", 
            MB_ICONERROR
        );

        return;
    }

    std::string fileContent = fmt::format(R"(<!doctype html><html style="width: 100%; height: 100%"><head><title>SharedJSContext</title><meta charset="utf-8"><script defer="defer" src="/libraries/{}"></script><script defer="defer" src="/library.js"></script><link href="/css/library.css" rel="stylesheet"></head><body style="width: 100%; height: 100%; margin: 0; overflow: hidden;"><div id="root" style="height:100%; width: 100%"></div><div style="display:none"></div></body></html>)", libraryChunkJS);

    try
    {
        SystemIO::WriteFileSync(SteamUIModulePath.string(), fileContent);
    }
    catch (const std::system_error& e)
    {
        Logger.Warn("Failed to restore SharedJSContext: {}", e.what());
    }
    catch (const std::exception& e)
    {
        Logger.Warn("Failed to restore SharedJSContext: {}", e.what());
    }

    Logger.Log("Restored SharedJSContext...");
    #endif
}


/**
 * Notifies the frontend of the backend load and handles script injection and state updates.
 * 
 * This function performs the following tasks:
 * 1. Logs the start of the backend load notification process.
 * 2. Sets up a message listener to handle the frontend's response, specifically waiting for a script identifier.
 * 3. Sends messages to enable the page and inject a script to the frontend for execution.
 * 4. Waits for a response from the frontend indicating successful script injection, then triggers a page reload.
 * 5. Logs completion of the process and removes the message listener.
 *
 * Synchronization:
 * - Uses a mutex and condition variable to ensure thread-safe waiting for the frontend's script injection acknowledgment.
 * 
 * Error Handling:
 * - If any issues occur during the message processing, errors are logged with details.
 */
void OnBackendLoad(bool reloadFrontend)
{
    UnPatchSharedJSContext(); // Restore the original SharedJSContext
    Logger.Log("Notifying frontend of backend load...");

    enum PageMessage
    {
        DEBUGGER_RESUME = 1,
        PAGE_ENABLE = 2,
        PAGE_SCRIPT = 3,
        PAGE_RELOAD = 4
    };

    JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", "OnBackendLoad", [reloadFrontend] (const nlohmann::json& eventMessage, std::string listenerId)
    {
        auto& state = BackendLoadState::get();
        std::unique_lock<std::mutex> lock(state.mtx);
        
        try
        {
            const PageMessage messageId = (PageMessage)(int)eventMessage.value("id", -1);

            if (messageId == PAGE_ENABLE)
            {
                Logger.Log("Injecting script to evaluate on new document...");
                Sockets::PostShared({ {"id", PAGE_SCRIPT }, {"method", "Page.addScriptToEvaluateOnNewDocument"}, {"params", {{ "source", ConstructOnLoadModule() }}} });
            }
            if (messageId == PAGE_SCRIPT)
            {
                Logger.Log("Script injected, waiting for identifier...");

                addedScriptOnNewDocumentId = eventMessage["result"]["identifier"];
                state.hasScriptIdentifier = true;
                Logger.Log("Successfully injected shims, reloading frontend...");

                if (reloadFrontend) Sockets::PostShared({ {"id", PAGE_RELOAD }, {"method", "Page.reload"}, { "params", { { "ignoreCache", true } }} });
                state.cvScript.notify_one();  

            }
            if (messageId == PAGE_RELOAD)
            {
                if (eventMessage.contains("error"))
                {
                    Logger.Log("Failed to reload frontend: {}", eventMessage["error"].dump(4));
                    if (reloadFrontend) Sockets::PostShared({ {"id", PAGE_RELOAD }, {"method", "Page.reload"}, { "params", { { "ignoreCache", true } }} });
                    return;
                }
                
                Logger.Log("Successfully notified frontend...");
                JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
            }
        }
        catch (nlohmann::detail::exception& ex)
        {
            LOG_ERROR("JavaScript::SharedJSMessageEmitter error -> {}", ex.what());
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Sockets::PostShared({ {"id", PAGE_ENABLE }, {"method", "Page.enable"} });
    {
        auto& state = BackendLoadState::get();
        std::unique_lock<std::mutex> lock(state.mtx);
        state.cvScript.wait(lock, [&state] { return state.hasScriptIdentifier; });
    }
    Logger.Log("Frontend notifier finished!");
}

/**
 * @brief Injects the frontend shims.
 * 
 * This function injects the frontend shims by registering a callback function to be called when the backend is loaded.
 * 
 */
const void CoInitializer::InjectFrontendShims(bool reloadFrontend) 
{
    BackendCallbacks& backendHandler = BackendCallbacks::getInstance();
    backendHandler.RegisterForLoad(std::bind(OnBackendLoad, reloadFrontend));
}

/**
 * @brief Re-injects the frontend shims.
 * 
 * This function re-injects the frontend shims by removing the existing script and injecting a new one.
 * 
 * @param {std::shared_ptr<PluginLoader>} pluginLoader - The plugin loader instance.
 */
const void CoInitializer::ReInjectFrontendShims(std::shared_ptr<PluginLoader> pluginLoader, bool reloadFrontend)
{
    pluginLoader->InjectWebkitShims();

    Sockets::PostShared({ {"id", 0 }, {"method", "Page.removeScriptToEvaluateOnNewDocument"}, {"params", {{ "identifier", addedScriptOnNewDocumentId }}} });
    InjectFrontendShims(reloadFrontend);
}
