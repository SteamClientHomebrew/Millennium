#include <vector>
#include <generic/stream_parser.h>
#include <Python.h>
#include <fmt/core.h>
#include <core/backend/co_spawn.hpp>
#include <utilities/log.hpp>
#include <core/loader.hpp>
#include <core/hooks/web_load.h>
#include <__builtins__/incbin.h>
#include <generic/base.h>

constexpr const char* bootstrap_module = R"(
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

// var originalConsoleLog = console.log;

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

/// @brief sets up the python interpretor to use virtual environment site packages, aswell as custom python path.
/// @param spath 
/// @return void 
const void add_site_packages(std::vector<std::string> spath) 
{
    PyObject *sys_module = PyImport_ImportModule("sys");
    if (!sys_module) {
        console.err("couldn't import sysmodule");
        return;
    }

    PyObject *sys_path = PyObject_GetAttrString(sys_module, "path");
    if (sys_path) {
#ifdef _WIN32
        PyList_SetSlice(sys_path, 0, PyList_Size(sys_path), NULL);
#endif

        for (const auto& path : spath) {
            PyList_Append(sys_path, PyUnicode_FromString(path.c_str()));
        }
        Py_DECREF(sys_path);
    }
    Py_DECREF(sys_module);
}

/// @brief initializes the current plugin. creates a plugin instance and calls _load()
/// @param global_dict 
void initialize_plugin(PyObject* global_dict) {
    PyObject *plugin_class = PyDict_GetItemString(global_dict, "Plugin");

    if (!plugin_class || !PyCallable_Check(plugin_class)) {
        PyErr_Print(); return;
    }

    PyObject *plugin_instance = PyObject_CallObject(plugin_class, NULL);

    if (!plugin_instance) {
        PyErr_Print(); return;
    }

    PyDict_SetItemString(global_dict, "plugin", plugin_instance);
    PyObject *load_method = PyObject_GetAttrString(plugin_instance, "_load");

    if (!load_method || !PyCallable_Check(load_method)) {
        PyErr_Print(); return;
    }

    PyObject_CallObject(load_method, NULL);
    Py_DECREF(load_method);
    Py_DECREF(plugin_instance);
}

void plugin_start_cb(stream_buffer::plugin_mgr::plugin_t& plugin) {

    const std::string _module = plugin.backend_abs.generic_string();
    const std::string base = plugin.base_dir.generic_string();

    PyObject* global_dict = PyModule_GetDict((PyObject*)PyImport_AddModule("__main__"));

    // bind the plugin name to the relative interpretor
    PyDict_SetItemString(global_dict, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(plugin.name.c_str()));

    std::vector<std::string> sys_paths = { 
        fmt::format("{}/backend", base),
#ifdef _WIN32
        pythonPath, pythonLibs
#endif
    };

    // include venv paths to interpretor
    if (plugin.pjson.contains("venv") && plugin.pjson["venv"].is_string()) 
    {
        const auto venv = fmt::format("{}/{}", base, plugin.pjson["venv"].get<std::string>());

        // configure virtual environment for the current plugin
        sys_paths.push_back(fmt::format("{}/Lib/site-packages", venv));    
        sys_paths.push_back(fmt::format("{}/Lib/site-packages/win32", venv));    
        sys_paths.push_back(fmt::format("{}/Lib/site-packages/win32/lib", venv));    
        sys_paths.push_back(fmt::format("{}/Lib/site-packages/Pythonwin", venv));    
    }

    add_site_packages(sys_paths);

#ifdef __linux__
    PyRun_SimpleString("import sys\nprint(sys.path)");
#endif

    PyObject *obj = Py_BuildValue("s", _module.c_str());
    FILE *file = _Py_fopen_obj(obj, "r+");

    if (file == NULL) {
        console.err("failed to fopen file @ {}", _module);
        return;
    }

    if (PyRun_SimpleFile(file, _module.c_str()) != 0) {
        console.err("millennium failed to startup [{}]", plugin.name);
        return;
    }

    initialize_plugin(global_dict);  
}

const std::string make_script(std::string filename) {
    return fmt::format("\nif (!document.querySelectorAll(`script[src='{}'][type='module']`).length) {{ document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module', id: 'millennium-injected' }})); }}", filename, filename);
}

const std::string setup_bootstrap_module() {
    
    auto plugins = stream_buffer::plugin_mgr::parse_all();
    std::string import_table;
    
    for (auto& plugin : plugins)  {

        if (!stream_buffer::plugin_mgr::is_enabled(plugin.name)) { 
            
            console.log("bootstrap skipping frontend {} [disabled]", plugin.name);
            continue;
        }
        import_table.append(make_script(fmt::format("https://steamloopback.host/{}", plugin.frontend_abs)));
    }

    std::string script = bootstrap_module;
    std::size_t pos = script.find("SCRIPT_RAW_TEXT");

    if (pos != std::string::npos) {
        // Replace the found "SCRIPT_RAW_TEXT" with import_table
        script.replace(pos, strlen("SCRIPT_RAW_TEXT"), import_table);
    }
    else {
        console.err("couldn't shim bootstrap module with plugin frontends. "
        "SCRIPT_RAW_TEXT was not found in the target string");
    }

    return script;
}

const void inject_shims(void) 
{
    tunnel::post_shared({ {"id", 1   }, {"method", "Page.enable"} });
    tunnel::post_shared({ {"id", 8567}, {"method", "Page.addScriptToEvaluateOnNewDocument"}, {"params", {{ "source", setup_bootstrap_module() }}} });
    // tunnel::post_shared({ {"id", 70  }, {"method", "Debugger.enable"} });
    // tunnel::post_shared({ {"id", 68  }, {"method", "Runtime.enable"} });
    // tunnel::post_shared({ {"id", 68  }, {"method", "Fetch.enable"} });
    tunnel::post_shared({ {"id", 69  }, {"method", "Page.reload"} });
    // tunnel::post_shared({ {"id", 71  }, {"method", "Debugger.pause"} });
}
