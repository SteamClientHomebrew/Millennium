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
        // reset sys path in case user already has python installed, to prevent conlficting versions
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
        PyErr_Print(); 
        return;
    }

    PyObject *plugin_instance = PyObject_CallObject(plugin_class, NULL);

    if (!plugin_instance) {
        PyErr_Print();
        return;
    }

    PyDict_SetItemString(global_dict, "plugin", plugin_instance);
    PyObject *load_method = PyObject_GetAttrString(plugin_instance, "_load");

    if (!load_method || !PyCallable_Check(load_method)) {
        PyErr_Print(); 
        return;
    }
    PyObject_CallObject(load_method, NULL);

    // Cleanup references
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
        // fmt::format("{}/.millennium/python.zip", stream_buffer::steam_path().generic_string()),
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
        // An error occurred during script execution
        console.err("millennium failed to startup [{}]", plugin.name);
        return;
    }

    initialize_plugin(global_dict);  
}

const std::string make_script(std::string filename) {
    return fmt::format("\nif (!document.querySelectorAll(`script[src='{}'][type='module']`).length) {{ document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module', id: 'millennium-injected' }})); }}", filename, filename);
}

const void inject_shims(void) 
{
    tunnel::post_shared({ {"id", 1}, {"method", "Page.enable"} });

    auto plugins = stream_buffer::plugin_mgr::parse_all();
    std::string _import;
    
    for (auto& plugin : plugins)  {

        if (!stream_buffer::plugin_mgr::is_enabled(plugin.name)) { 
            
            console.log("bootstrap skipping frontend {} [disabled]", plugin.name);
            continue;
        }

        // steam's cef has a loopback for files by default, so we can just use that
        _import.append(make_script(fmt::format("https://steamloopback.host/{}", plugin.frontend_abs)));
    }

    std::string script = fboot_script;

    // script.replace(script.find("API_RAW_TEXT"), sizeof("API_RAW_TEXT") - 1, API);
    script.replace(script.find("SCRIPT_RAW_TEXT"), sizeof("SCRIPT_RAW_TEXT") - 1, _import);

    tunnel::post_shared({ {"id", 8567}, {"method", "Page.addScriptToEvaluateOnNewDocument"}, {"params", {{ "source", script }}} });
    tunnel::post_shared({ {"id", 69  }, {"method", "Page.reload"} });
}
