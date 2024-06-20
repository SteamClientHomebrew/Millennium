#include "co_stub.h"
#include <vector>
#include <Python.h>
#include <fmt/core.h>
#include <core/py_controller/co_spawn.h>
#include <sys/log.h>
#include <core/loader.h>
#include <core/hooks/web_load.h>
#include <core/ffi/ffi.h>

constexpr const char* bootstrapModule = R"(
function createWebSocket(url) {
    return new Promise((resolve, reject) => {
        let socket = new WebSocket(url);

        socket.addEventListener('open', () => {
            console.log('WebSocket connected');
            resolve(socket);
        });

        socket.addEventListener('error', (error) => {
            console.error('WebSocket error:', error);
            reject(error);
        });

        socket.addEventListener('close', () => {
            console.warn('WebSocket closed, attempting to reconnect...');
            setTimeout(() => {
                createWebSocket(url).then(resolve).catch(reject);
            }, 100);
        });
    });
}

function waitForSocket(socket) {
    return new Promise((resolve, reject) => {
        if (socket.readyState === WebSocket.OPEN) {
            resolve();
        } else {
            socket.addEventListener('open', resolve);
            socket.addEventListener('error', reject);
        }
    });
}

const InjectLegacyReactGlobals = () => 
{
    let initReq;
    let bufferWebpackCache = {};

    window.webpackChunksteamui?.push([[Math.random()], {}, (r) => { initReq = r; }]);

    for (let i of Object.keys(initReq.m)) {
        try {
            bufferWebpackCache[i] = initReq(i);
        } catch (e) {
            console.debug("[Millennium:Webpack]: Ignoring require error for module", i, e);
        }
    }

    const findModule = (filter) => {
        const allModules = Object.values(bufferWebpackCache).filter((x) => x)

        for (const m of allModules) {
            if (m.default && filter(m.default)) return m.default;
            if (filter(m)) return m;
        }
    };

    window.SP_REACTDOM = findModule((module) => module.createPortal && module.createRoot && module.flushSync)
    window.SP_REACT    = findModule((module) => module.Component && module.PureComponent && module.useLayoutEffect);
}

function waitForSPReactDOM() {
    return new Promise((resolve) => {
        const interval = setInterval(() => {
            if (window?.webpackChunksteamui?.length > 3) {
                InjectLegacyReactGlobals()
                clearInterval(interval);
                resolve();
            }
        }, 100);
    });
}

createWebSocket('ws://localhost:12906').then((socket) => {
    window.MILLENNIUM_IPC_SOCKET = socket;
    window.CURRENT_IPC_CALL_COUNT = 0;

    Promise.all([ waitForSocket(socket), waitForSPReactDOM() ]).then(() => {
        console.log('%c Millennium ', 'background: black; color: white', "Reading to inject shims...");
        SCRIPT_RAW_TEXT
    })
    .catch((error) => console.error('error @ createWebSocket ->', error));
})
.catch((error) => console.error('Initial WebSocket connection failed:', error));
)";

/// @brief sets up the python interpreter to use virtual environment site packages, as well as custom python path.
/// @param system path 
/// @return void 
const void AddSitePackages(std::vector<std::filesystem::path> spath) 
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

const void CoInitializer::BackendStartCallback(SettingsStore::PluginTypeSchema plugin) {

    const std::string backendMainModule = plugin.backendAbsoluteDirectory.generic_string();

    PyObject* globalDictionary = PyModule_GetDict((PyObject*)PyImport_AddModule("__main__"));
    PyDict_SetItemString(globalDictionary, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(plugin.pluginName.c_str()));

    std::vector<std::filesystem::path> sysPath = 
    { 
        plugin.pluginBaseDirectory / "backend", 
        pythonPath, 
        pythonLibs
    };

    if (plugin.pluginJson.contains("venv") && plugin.pluginJson["venv"].is_string()) 
    {
        const auto pluginVirtualEnv = plugin.pluginBaseDirectory / plugin.pluginJson["venv"];

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
        LOG_ERROR("failed to fopen file @ {}", backendMainModule);
        return;
    }

    if (PyRun_SimpleFile(mainModuleFilePtr, backendMainModule.c_str()) != 0) 
    {
        LOG_ERROR("millennium failed to startup [{}]", plugin.pluginName);
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
            continue;
        }

        scriptImportTable.append(ConstructScriptElement(fmt::format("https://steamloopback.host/{}", plugin.frontendAbsoluteDirectory.generic_string())));
    }

    std::string scriptLoader = bootstrapModule;
    std::size_t importTablePos = scriptLoader.find("SCRIPT_RAW_TEXT");

    if (importTablePos != std::string::npos) 
    {
        scriptLoader.replace(importTablePos, strlen("SCRIPT_RAW_TEXT"), scriptImportTable);
    }
    else 
    {
        LOG_ERROR("couldn't shim bootstrap module with plugin frontends. "
        "SCRIPT_RAW_TEXT was not found in the target string");
    }

    return scriptLoader;
}

static std::string addedScriptOnNewDocumentId;

const void CoInitializer::InjectFrontendShims(void)
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
            LOG_ERROR(fmt::format("JavaScript::SharedJSMessageEmitter error -> {}", ex.what()));
        }
    });
}

const void CoInitializer::ReInjectFrontendShims()
{
    Sockets::PostShared({ {"id", 0 }, {"method", "Page.removeScriptToEvaluateOnNewDocument"}, {"params", {{ "identifier", addedScriptOnNewDocumentId }}} });
    InjectFrontendShims();
}
