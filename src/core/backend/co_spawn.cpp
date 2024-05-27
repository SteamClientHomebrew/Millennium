#include <Python.h>
#include <core/loader.hpp>
#include <__builtins__/executor.h>
#include <core/impl/mutex_impl.hpp>
#include "co_spawn.hpp"
#include "bind_stdout.hpp"
#include <boxer/boxer.h>
#include <generic/base.h>
#include <thread>

static struct PyModuleDef module_def = { PyModuleDef_HEAD_INIT, "Millennium", NULL, -1, (PyMethodDef*)get_module_handle() };

PyMODINIT_FUNC PyInit_Millennium(void) {
    return PyModule_Create(&module_def);
}

plugin_manager::plugin_manager() 
{
    console.head("init plugin manager:");
    this->instance_count = 0;

    // initialize global modules
    console.log_item("hook", "locking standard output..."); PyImport_AppendInittab("hook_stdout", &PyInit_custom_stdout);
    console.log_item("hook", "locking standard error...");  PyImport_AppendInittab("hook_stderr", &PyInit_custom_stderr);
    console.log_item("hook", "inserting Millennium...");    PyImport_AppendInittab("Millennium", &PyInit_Millennium);

    console.log_item("status", "done appending init tabs!");
    console.log_item("status", "initializing python...");

    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    /* Read all configuration at once */
    status = PyConfig_Read(&config);

    if (PyStatus_Exception(status)) {
        console.err("couldn't read config {}", status.err_msg);
        goto done;
    }

    config.write_bytecode = 0;

#ifdef _WIN32
    config.module_search_paths_set = 1;

    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonPath.begin(), pythonPath.end()).c_str());
    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonLibs.begin(), pythonLibs.end()).c_str());

#endif

    status = Py_InitializeFromConfig(&config);

    if (PyStatus_Exception(status)) {
        console.err("couldn't initialize from config {}", status.err_msg);
        goto done;
    }

    _save = PyEval_SaveThread();
done:
    PyConfig_Clear(&config);
}

plugin_manager::~plugin_manager()
{
    console.err("::~plugin_manager() was destroyed.");
    PyEval_RestoreThread(_save);
    Py_FinalizeEx();
}

bool plugin_manager::create_instance(stream_buffer::plugin_mgr::plugin_t& plugin, std::function<void(stream_buffer::plugin_mgr::plugin_t&)> callback)
{
    const std::string name = plugin.name;
    this->instance_count++;

    auto thread = std::thread([this, name, callback, &plugin] 
    {
        PyThreadState* auxts = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(auxts);
        PyThreadState* subts = Py_NewInterpreter();

        PyThreadState_Swap(subts);
        {
            this->instances_.push_back({ name, subts, auxts });
            redirect_output();
            callback(plugin);
        }
        //Py_EndInterpreter(subts); closes intepretor

        PyThreadState_Clear(auxts);
        PyThreadState_Swap(auxts);
        PyThreadState_DeleteCurrent();
    });

    this->thread_pool.push_back(std::move(thread));
    return true;
}

bool plugin_manager::shutdown_plugin(std::string plugin_name)
{
    bool success = false;

    for (auto it = this->instances_.begin(); it != this->instances_.end(); ++it) {
        const auto& [name, thread_ptr, _auxts] = *it;

        if (name != plugin_name) {
            continue;
        }
        
        success = true;

        if (thread_ptr == nullptr) {
            console.err(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
            success = false;
        }

        PyThreadState* auxts = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(auxts);
        PyGILState_STATE gstate = PyGILState_Ensure();
        PyThreadState_Swap(thread_ptr);

        if (thread_ptr == NULL) {
            console.err("script execution was queried but the receiving parties thread state was nullptr");
            success = false;
        }

        if (PyRun_SimpleString("plugin._unload()") != 0) {
            PyErr_Print();
            console.err("millennium failed to shutdown [{}]", plugin_name);
            success = false;
        }

        // close the python interpretor. GIL must be held and the interpretor must be IDLE
        Py_EndInterpreter(thread_ptr);
        PyThreadState_Clear(auxts);
        PyThreadState_Swap(auxts);
        PyGILState_Release(gstate);
        PyThreadState_DeleteCurrent();

        this->instances_.erase(it);
        break;
    }

    console.log("Successfully shut down [{}].\nRemaining: ", plugin_name);
    
    for (const auto& plugin: this->instances_) {
        console.log(plugin.name);
    }

    javascript::reload_shared_context();
    return true;
}

void plugin_manager::wait_for_exit()
{
    for (auto& thread : this->thread_pool) {
        thread.detach();
    }
}

thread_state plugin_manager::get_thread_state(std::string pluginName)
{
    for (const auto& [name, thread_ptr, auxts] : this->instances_) {
        if (name == pluginName) {
            return {
                name, thread_ptr, auxts
            };
        }
    }
    return {};
}

std::string plugin_manager::get_plugin_name(PyThreadState* thread) 
{
    for (const auto& [name, thread_ptr, auxts] : this->instances_) {

        if (thread_ptr == nullptr) {
            console.err("thread_ptr was nullptr");
            return {};
        }

        if (thread_ptr == thread) return name;  
    }
    return {};
}