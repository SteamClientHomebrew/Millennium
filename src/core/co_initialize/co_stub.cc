#include "co_stub.h"
#include <vector>
#include <Python.h>
#include <fmt/core.h>
#include <core/py_controller/co_spawn.hpp>
#include <utilities/log.hpp>
#include <core/loader.hpp>
#include <core/hooks/web_load.h>
#include <generic/base.h>
#include <core/ffi/ffi.hpp>

constexpr const char* bootstrapModule = R"(
/**
 * This module is in place to load all plugin frontends
 * @ src\__builtins__\bootstrap.js
 */

class EventEmitter {
    constructor() {
      this.events = {};
      this.pendingEvents = {}; // Store emitted events temporarily
    }
  
    on(eventName, listener) {
      this.events[eventName] = this.events[eventName] || [];
      this.events[eventName].push(listener);
  
      // If there are pending events for this eventName, emit them now
      if (this.pendingEvents[eventName]) {
        this.pendingEvents[eventName].forEach(args => this.emit(eventName, ...args));
        delete this.pendingEvents[eventName];
      }
    }
  
    emit(eventName, ...args) {
      if (this.events[eventName]) {
        this.events[eventName].forEach(listener => listener(...args));
      } else {
        // If there are no listeners yet, store the emitted event for later delivery
        this.pendingEvents[eventName] = this.pendingEvents[eventName] || [];
        this.pendingEvents[eventName].push(args);
      }
    }
  
    off(eventName, listener) {
      if (this.events[eventName]) {
        this.events[eventName] = this.events[eventName].filter(registeredListener => registeredListener !== listener);
      }
    }
}
const emitter = new EventEmitter();
console.log('%c Millennium ', 'background: black; color: white', "Bootstrapping modules...");

// connect to millennium websocket IPC
window.CURRENT_IPC_CALL_COUNT = 0;

function connectIPCWebSocket() {
    window.MILLENNIUM_IPC_SOCKET = new WebSocket('ws://localhost:12906');

    window.MILLENNIUM_IPC_SOCKET.onopen = function(event) {
        console.log('%c Millennium ', 'background: black; color: white', "Successfully connected to Millennium!");
        emitter.emit('loaded', true);
    };

    window.MILLENNIUM_IPC_SOCKET.onclose = function(event) {
        console.error('IPC connection was peacefully broken.', event);
        setTimeout(connectIPCWebSocket, 100); 
    };

    window.MILLENNIUM_IPC_SOCKET.onerror = function(error) {
        console.error('IPC connection was forcefully broken.', error);
    };
}

connectIPCWebSocket();

/**
 * seemingly fully functional, though needs a rewrite as its tacky
 * @todo hook a function that specifies when react is loaded, or use other methods to figure out when react has loaded. 
 * @param {*} message 
 */
function __millennium_module_init__() {
  if (window.SP_REACTDOM === undefined) {
    setTimeout(() => __millennium_module_init__(), 1)
  }
  else {
    console.log('%c Millennium ', 'background: black; color: white', "Bootstrapping builtin modules...");
    // this variable points @ src_builtins_\api.js

    emitter.on('loaded', () => {
      SCRIPT_RAW_TEXT
    });
  }
}
__millennium_module_init__()
)";

/// @brief sets up the python interpreter to use virtual environment site packages, as well as custom python path.
/// @param system path 
/// @return void 
const void AddSitePackages(std::vector<std::filesystem::path> spath) 
{
    PyObject *sysModule = PyImport_ImportModule("sys");
    if (!sysModule) 
    {
        Logger.Error("couldn't import system module");
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

        for (const auto& systemPathItem : spath) 
        {
            PyList_Append(systemPath, PyUnicode_FromString(systemPathItem.generic_string().c_str()));
        }
        Py_DECREF(systemPath);
    }
    Py_DECREF(sysModule);
}

/// @brief initializes the current plugin. creates a plugin instance and calls _load()
/// @param global_dict 
void StartPluginBackend(PyObject* global_dict) 
{
    PyObject *pluginComponent = PyDict_GetItemString(global_dict, "Plugin");

    if (!pluginComponent || !PyCallable_Check(pluginComponent)) 
    {
        PyErr_Print(); 
        return;
    }

    PyObject *pluginComponentInstance = PyObject_CallObject(pluginComponent, NULL);

    if (!pluginComponentInstance) 
    {
        PyErr_Print(); 
        return;
    }

    PyDict_SetItemString(global_dict, "plugin", pluginComponentInstance);
    PyObject *loadMethodAttribute = PyObject_GetAttrString(pluginComponentInstance, "_load");

    if (!loadMethodAttribute || !PyCallable_Check(loadMethodAttribute)) 
    {
        PyErr_Print(); 
        return;
    }

    PyObject_CallObject(loadMethodAttribute, NULL);
    Py_DECREF(loadMethodAttribute);
    Py_DECREF(pluginComponentInstance);
}

void BackendStartCallback(SettingsStore::PluginTypeSchema& plugin) {

    const std::string backendMainModule = plugin.backendAbsoluteDirectory.generic_string();

    PyObject* globalDictionary = PyModule_GetDict((PyObject*)PyImport_AddModule("__main__"));
    PyDict_SetItemString(globalDictionary, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(plugin.pluginName.c_str()));

    std::vector<std::filesystem::path> sysPath = { 
        plugin.pluginBaseDirectory / "backend", pythonPath, pythonLibs
    };

    // include venv paths to interpretor
    if (plugin.pluginJson.contains("venv") && plugin.pluginJson["venv"].is_string()) 
    {
        const auto pluginVirtualEnv = plugin.pluginBaseDirectory / plugin.pluginJson["venv"];

        // configure virtual environment for the current plugin
        sysPath.push_back(pluginVirtualEnv / "Lib" / "site-packages");
        sysPath.push_back(pluginVirtualEnv / "Lib" / "site-packages" / "win32");
        sysPath.push_back(pluginVirtualEnv / "Lib" / "site-packages" / "win32" / "Lib");
        sysPath.push_back(pluginVirtualEnv / "Lib" / "site-packages" / "Pythonwin");
    }

    AddSitePackages(sysPath);

    PyObject *mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
    FILE *mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r+");

    if (mainModuleFilePtr == NULL) 
    {
        Logger.Error("failed to fopen file @ {}", backendMainModule);
        return;
    }

    if (PyRun_SimpleFile(mainModuleFilePtr, backendMainModule.c_str()) != 0) 
    {
        Logger.Error("millennium failed to startup [{}]", plugin.pluginName);
        return;
    }

    StartPluginBackend(globalDictionary);  
    //fclose(mainModuleFilePtr);
}

const std::string ConstructScriptElement(std::string filename) 
{
    return fmt::format("\nif (!document.querySelectorAll(`script[src='{}'][type='module']`).length) {{ document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module', id: 'millennium-injected' }})); }}", filename, filename);
}

const std::string ConstructOnLoadModule() 
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    std::vector<SettingsStore::PluginTypeSchema> plugins = settingsStore->ParseAllPlugins();

    std::string scriptImportTable;
    
    for (auto& plugin : plugins)  
    {
        if (!settingsStore->IsEnabledPlugin(plugin.pluginName)) 
        {    
            Logger.Log("bootstrap skipping frontend {} [disabled]", plugin.pluginName);
            continue;
        }

        scriptImportTable.append(ConstructScriptElement(fmt::format("https://steamloopback.host/{}", plugin.frontendAbsoluteDirectory)));
    }

    std::string scriptLoader = bootstrapModule;
    std::size_t importTablePos = scriptLoader.find("SCRIPT_RAW_TEXT");

    if (importTablePos != std::string::npos) 
    {
        scriptLoader.replace(importTablePos, strlen("SCRIPT_RAW_TEXT"), scriptImportTable);
    }
    else 
    {
        Logger.Error("couldn't shim bootstrap module with plugin frontends. "
        "SCRIPT_RAW_TEXT was not found in the target string");
    }

    return scriptLoader;
}

static std::string addedScriptOnNewDocumentId;

const void InjectFrontendShims(void) 
{
    enum PageMessage
    {
        PAGE_ENABLE,
        PAGE_SCRIPT,
        PAGE_RELOAD
    };

    Sockets::PostShared({ {"id", PAGE_ENABLE }, {"method", "Page.enable"} });
    Sockets::PostShared({ {"id", PAGE_SCRIPT }, {"method", "Page.addScriptToEvaluateOnNewDocument"}, {"params", {{ "source", ConstructOnLoadModule() }}} });
    Sockets::PostShared({ {"id", PAGE_RELOAD }, {"method", "Page.reload"} });

    JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", [&](const nlohmann::json& eventMessage, int listenerId)
    {
        try
        {
            if (eventMessage.contains("id") && eventMessage["id"] != PAGE_SCRIPT)
            {
                return;
            }

            addedScriptOnNewDocumentId = eventMessage["result"]["identifier"];
            JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
        }
        catch (nlohmann::detail::exception& ex)
        {
            Logger.Error(fmt::format("JavaScript::SharedJSMessageEmitter error -> {}", ex.what()));
        }
    });
}

const void ReInjectFrontendShims()
{
    Sockets::PostShared({ {"id", 0 }, {"method", "Page.removeScriptToEvaluateOnNewDocument"}, {"params", {{ "identifier", addedScriptOnNewDocumentId }}} });
    InjectFrontendShims();
}
