#include "co_spawn.hpp"
#include <thread>
#include <iostream>
#include <Python.h>

#include "bind_stdout.hpp"
#include <__builtins__/executor.h>
#include <future>

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

bool plugin_manager::create_instance(std::string name, std::function<void()> callback)
{
    this->instance_count++;
    auto thread = std::thread([this, name, callback] {
        PyThreadState* auxts = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(auxts);

        PyThreadState* subts = Py_NewInterpreter();
        console.log("hosting [{}] thread_s_ptr @ {}", name, static_cast<void*>(&subts));

        PyThreadState_Swap(subts);
        {
            this->instances_.push_back({ name, subts, auxts });

            redirect_output();
            callback();
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

    for (const auto& [name, thread_ptr, _auxts] : this->instances_) {

        if (name != plugin_name) {
            continue;
        }

        success = true;

        if (thread_ptr == nullptr) {
            console.err(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
            return false;
        }

        PyThreadState* auxts = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(auxts);

        // switch global lock to current thread swap
        PyGILState_STATE gstate = PyGILState_Ensure();

        // switch thread state to respective backend 
        PyThreadState_Swap(thread_ptr);

        if (thread_ptr == NULL) {
            console.err("script execution was queried but the receiving parties thread state was nullptr");
            return false;
        }

        // synchronously call the unload plugin method and wait for exit.
        PyRun_SimpleString("plugin._unload()");

        // force close the python interpretor. 
        Py_EndInterpreter(thread_ptr);
        PyThreadState_Clear(auxts);
        PyThreadState_Swap(auxts);

        PyGILState_Release(gstate);
        PyThreadState_DeleteCurrent();
    }
    return success;
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