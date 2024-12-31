#include "co_stub.h"
#include <vector>
#include <Python.h>
#include <fmt/core.h>
#include <core/py_controller/co_spawn.h>
#include <sys/log.h>
#include <core/loader.h>
#include <core/hooks/web_load.h>
#include <core/ffi/ffi.h>
#include <tuple>

const std::string GetBootstrapModule(const std::vector<std::string> scriptModules, const uint16_t port)
{
    std::string scriptModuleArray;
    std::string scriptContents = SystemIO::ReadFileSync((SystemIO::GetInstallPath() / "ext" / "data" / "shims" / "client_api.js").string());

    if (scriptContents.empty())
    {
        LOG_ERROR("Missing webkit preload module. Please re-install Millennium.");
        #ifdef _WIN32
        MessageBoxA(NULL, "Missing client preload module. Please re-install Millennium.", "Millennium", MB_ICONERROR);
        #endif
    }

    for (int i = 0; i < scriptModules.size(); i++)
    {
        scriptModuleArray.append(fmt::format("\"{}\"{}", scriptModules[i], (i == scriptModules.size() - 1 ? "" : ",")));
    }

    return fmt::format("{}\nmillennium_components({}, [{}]);", scriptContents, port, scriptModuleArray);
}

/// @brief sets up the python interpreter to use virtual environment site packages, as well as custom python path.
/// @param system path 
/// @return void 
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


/// @brief initializes the current plugin. creates a plugin instance and calls _load()
/// @param global_dict 
void StartPluginBackend(PyObject* global_dict, std::string pluginName) 
{
    const auto PrintError = [&pluginName]() 
    {
        const auto [errorMessage, traceback] = Python::GetExceptionInformaton();
        PyErr_Clear();

        if (errorMessage == "name 'plugin' is not defined")
        {
            Logger.PrintMessage(" FFI-ERROR ", fmt::format("Millennium failed to call _load on {}", pluginName), COL_RED);
            return;
        }

        Logger.PrintMessage(" FFI-ERROR ", fmt::format("Millennium failed to call _load on {}: {}\n{}{}", pluginName, COL_RED, traceback, COL_RESET), COL_RED);
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

    PyObject_CallObject(loadMethodAttribute, NULL);
    Py_DECREF(loadMethodAttribute);
    Py_DECREF(pluginComponentInstance);
}

const void SetPluginSecretName(PyObject* globalDictionary, const std::string& pluginName) 
{
    PyDict_SetItemString(globalDictionary, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(pluginName.c_str()));
}

const void SetPluginEnvironmentVariables(PyObject* globalDictionary, const SettingsStore::PluginTypeSchema& plugin) 
{
    PyDict_SetItemString(globalDictionary, "PLUGIN_BASE_DIR", PyUnicode_FromString(plugin.pluginBaseDirectory.generic_string().c_str()));
    PyDict_SetItemString(globalDictionary, "__file__", PyUnicode_FromString((plugin.backendAbsoluteDirectory / "main.py").generic_string().c_str()));
}

const void CoInitializer::BackendStartCallback(SettingsStore::PluginTypeSchema plugin) 
{
    PyObject* globalDictionary = PyModule_GetDict(PyImport_AddModule("__main__"));

    const auto backendMainModule = plugin.backendAbsoluteDirectory.generic_string();
    const auto pluginVirtualEnv  = plugin.pluginBaseDirectory / ".millennium";

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

    #ifdef _WIN32
    AddSitePackagesDirectory(SystemIO::GetInstallPath() / "ext" / "data" / "cache" / "Lib" / "site-packages");
    #else
    AddSitePackagesDirectory(SystemIO::GetInstallPath() / "ext" / "data" / "cache" / "lib" / "python3.11" / "site-packages");
    #endif

    AddSitePackagesDirectory(pluginVirtualEnv);
    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();

    PyObject *mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
    FILE *mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r+");

    if (mainModuleFilePtr == NULL) 
    {
        Logger.Warn("failed to fopen file @ {}", backendMainModule);
        backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });
        return;
    }

    PyObject* mainModule = PyImport_AddModule("__main__");
    PyObject* mainModuleDict = PyModule_GetDict(mainModule);

    if (!mainModule || !mainModuleDict) {
        Logger.Warn("Millennium failed to initialize the main module.");
        backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });
        fclose(mainModuleFilePtr);
        return;
    }

    PyObject* result = PyRun_File(mainModuleFilePtr, backendMainModule.c_str(), Py_file_input, mainModuleDict, mainModuleDict);
    fclose(mainModuleFilePtr);

    if (!result) 
    {
        const auto [errorMessage, traceback] = Python::GetExceptionInformaton();

        Logger.PrintMessage(" PY-MAN ", fmt::format("Millennium failed to start {}: {}\n{}{}", plugin.pluginName, COL_RED, traceback, COL_RESET), COL_RED);
        Logger.Warn("Millennium failed to start '{}'. This is likely due to failing module side effects, unrelated to Millennium.", plugin.pluginName);
        backendHandler.BackendLoaded({ plugin.pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_FAILED });
        return;
    }

    Py_DECREF(result);
    StartPluginBackend(globalDictionary, plugin.pluginName);  
}

const std::string ConstructOnLoadModule(uint16_t ftpPort, uint16_t ipcPort) 
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
        const std::string pathShim = plugin.isInternal ? "_internal_/" : std::string();

        scriptImportTable.push_back(fmt::format("http://localhost:{}/{}{}", ftpPort, pathShim, frontEndAbs));
    }

    return GetBootstrapModule(scriptImportTable, ipcPort);
}

static std::string addedScriptOnNewDocumentId = "";

#include <mutex>
#include <condition_variable>
#include <thread>

void OnBackendLoad(uint16_t ftpPort, uint16_t ipcPort)
{
    Logger.Log("Notifying frontend of backend load...");

    static uint16_t m_ftpPort = ftpPort;
    static uint16_t m_ipcPort = ipcPort;

    enum PageMessage
    {
        DEBUGGER_RESUME = 1,
        PAGE_ENABLE = 2,
        PAGE_SCRIPT = 3,
        PAGE_RELOAD = 4
    };

    std::mutex mtx;
    std::condition_variable cvDebugger;
    std::condition_variable cvScript;

    bool hasUnpausedDebugger = false;
    bool hasScriptIdentifier = false;

    auto socketEmitterThread = std::thread([&]
    {
        JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", "OnBackendLoad", [&](const nlohmann::json& eventMessage, std::string listenerId)
        {
            std::unique_lock<std::mutex> lock(mtx);
            
            try
            {
                const int messageId = eventMessage.value("id", -1);

                if (messageId == DEBUGGER_RESUME)
                {
                    if (eventMessage.contains("error"))
                    {
                        hasUnpausedDebugger = false;
                        Logger.Warn("Failed to resume debugger, Steam is likely not yet loaded...");
                    }
                    else if (eventMessage.contains("result"))
                    {
                        hasUnpausedDebugger = true;
                        Logger.Log("Successfully resumed debugger, injecting shims...");
                        Sockets::PostShared({ {"id", PAGE_ENABLE }, {"method", "Page.enable"} });
                        Sockets::PostShared({ {"id", PAGE_SCRIPT }, {"method", "Page.addScriptToEvaluateOnNewDocument"}, {"params", {{ "source", ConstructOnLoadModule(m_ftpPort, m_ipcPort) }}} });
                    }
                    cvDebugger.notify_one();  // Notify that debugger resume is processed
                }
                else if (messageId == PAGE_SCRIPT)
                {
                    addedScriptOnNewDocumentId = eventMessage["result"]["identifier"];
                    hasScriptIdentifier = true;
                    Logger.Log("Successfully injected shims, updating state...");
                    Sockets::PostShared({ {"id", PAGE_RELOAD }, {"method", "Page.reload"} });
                    cvScript.notify_one();  // Notify that script injection is processed
                }

                // Check if both conditions have been met
                if (hasUnpausedDebugger && hasScriptIdentifier)
                {
                    Logger.Log("Successfully notified frontend...");
                    JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
                }
            }
            catch (nlohmann::detail::exception& ex)
            {
                LOG_ERROR("JavaScript::SharedJSMessageEmitter error -> {}", ex.what());
            }
        });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Sockets::PostShared({ {"id", DEBUGGER_RESUME }, {"method", "Debugger.resume"} });

    // Wait for debugger resume
    {
        std::unique_lock<std::mutex> lock(mtx);
        cvDebugger.wait(lock, [&] { return hasUnpausedDebugger; });
    }

    // Wait for script injection completion
    {
        std::unique_lock<std::mutex> lock(mtx);
        cvScript.wait(lock, [&] { return hasScriptIdentifier; });
    }

    socketEmitterThread.join();
    Logger.Log("Frontend notifier finished!");
}

const void CoInitializer::InjectFrontendShims(uint16_t ftpPort, uint16_t ipcPort) 
{
    std::mutex mtx;
    std::condition_variable cv;
    bool hasSuccess = false, hasPaused = false;

    Logger.Log("Preparing to inject frontend shims...");

    JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", "InjectFrontendShims", [&](const nlohmann::json& eventMessage, std::string listenerId) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        
        if (eventMessage.value("id", -1) == 65756) 
        {
            if (eventMessage.contains("error")) 
            {
                Logger.Warn("Failed to pause debugger, Steam is likely not yet loaded...");
                Sockets::PostShared({ {"id", 65756 }, {"method", "Debugger.pause"} });
            } 
            else if (eventMessage.contains("result")) 
            {
                Logger.Log("Successfully sent debugger pause...");
                hasSuccess = true;
            }
        }
        if (eventMessage.contains("method") && eventMessage["method"] == "Debugger.paused") 
        {
            Logger.Log("Debugger has paused!");
            hasPaused = true;
        }

        if (hasSuccess && hasPaused) 
        {
            try 
            {
                JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
            } 
            catch (const std::exception& ex) 
            {
                LOG_ERROR("Error removing listener: {}", ex.what());
            }
            cv.notify_all(); 
        }
    });

    Sockets::PostShared({ {"id", 3422 }, {"method", "Debugger.enable"} });
    Sockets::PostShared({ {"id", 65756 }, {"method", "Debugger.pause"} });

    try 
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return hasSuccess && hasPaused; });
    } 
    catch (const std::system_error& e)
    {
        LOG_ERROR("Condition variable wait error: {}", e.what());
        return;
    }

    Logger.Log("Ready to inject shims!");
    BackendCallbacks& backendHandler = BackendCallbacks::getInstance();
    backendHandler.RegisterForLoad(std::bind(OnBackendLoad, ftpPort, ipcPort));
}

const void CoInitializer::ReInjectFrontendShims()
{
    Sockets::PostShared({ {"id", 0 }, {"method", "Page.removeScriptToEvaluateOnNewDocument"}, {"params", {{ "identifier", addedScriptOnNewDocumentId }}} });
    InjectFrontendShims();
}
