#include <core/loader.hpp>
#include "co_spawn.hpp"
#include <thread>
#include <iostream>
#include <Python.h>

#include "bind_stdout.hpp"
#include <__builtins__/executor.h>
#include <core/impl/mutex_impl.hpp>
#include <future>
#include <boxer/boxer.h>

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "Millennium", NULL, -1, (PyMethodDef*)get_module_handle()
};

PyMODINIT_FUNC PyInit_Millennium(void) {
    return PyModule_Create(&module_def);
}

plugin_manager::plugin_manager() 
{
    this->instance_count = 0;

    // initialize global modules
    PyImport_AppendInittab("hook_stdout", &PyInit_custom_stdout);
    PyImport_AppendInittab("hook_stderr", &PyInit_custom_stderr);
    PyImport_AppendInittab("Millennium", &PyInit_Millennium);

    Py_InitializeEx(0); // implies PyEval_InitThreads()

    _save = PyEval_SaveThread();
}

plugin_manager::~plugin_manager()
{
    PyEval_RestoreThread(_save);
    Py_FinalizeEx();
}

bool plugin_manager::create_instance(stream_buffer::plugin_mgr::plugin_t& plugin, std::function<void(stream_buffer::plugin_mgr::plugin_t&)> callback)
{
    const std::string name = plugin.name;

    this->instance_count++;
    auto thread = std::thread([this, name, callback, &plugin] {
        PyThreadState* auxts = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(auxts);

        PyThreadState* subts = Py_NewInterpreter();
        // console.log("hosting [{}] thread_s_ptr @ {}", name, static_cast<void*>(&subts));

        PyThreadState_Swap(subts);
        {
            this->instances_.push_back({ name, subts, auxts });

            redirect_output();
            callback(plugin);
        }
        // uncomment to force main thread to be open
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

        // switch global lock to current thread swap
        PyGILState_STATE gstate = PyGILState_Ensure();

        // switch thread state to respective backend 
        PyThreadState_Swap(thread_ptr);

        if (thread_ptr == NULL) {
            console.err("script execution was queried but the receiving parties thread state was nullptr");
            success = false;
        }

        std::cout << "calling unload" << std::endl;
        // synchronously call the unload plugin method and wait for exit.
        if (PyRun_SimpleString("plugin._unload()") != 0) {
            PyErr_Print();
            console.err("millennium failed to shutdown [{}]", plugin_name);
            success = false;
        }

        // if (PyGILState_Check()) {
        //     const auto message = fmt::format("Couldn't shutdown [{}] because it is still running. plugin._unload() should break all loops/event listeners/any code asynchronously running\n\n"
        //     "If you are the end user and don't develop this plugin, contact the respective developer. (plugin developer, not Millennium developers)", plugin_name);

        //     boxer::show(message.c_str(), "Whoops!", boxer::Style::Error);
        //     success = false;
        //     return false;
        // }

        // close the python interpretor. GIL must be held and the interpretor must be IDLE
        Py_EndInterpreter(thread_ptr);
        PyThreadState_Clear(auxts);
        PyThreadState_Swap(auxts);

        PyGILState_Release(gstate);
        PyThreadState_DeleteCurrent();

        // Remove the plugin from the list
        this->instances_.erase(it);
        break; // Exit loop since we found and processed the plugin
    }

    console.log("Successfully shut down [{}].\nRemaining: ", plugin_name);
    
    for (const auto& plugin: this->instances_) {
        const auto& [name, thread_ptr, _auxts] = plugin;

        console.log(name);
    }

    //stream_buffer::plugin_mgr::set_plugin_status(plugin_name, false);
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

        if (thread_ptr == thread) {
            return name;
        }
    }
    return {};
}