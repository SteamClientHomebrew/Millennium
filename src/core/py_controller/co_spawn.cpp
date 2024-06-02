#include <Python.h>
#include <core/loader.hpp>
#include <__builtins__/executor.h>
#include <core/ffi/ffi.hpp>
#include "co_spawn.hpp"
#include "bind_stdout.hpp"
#include <boxer/boxer.h>
#include <generic/base.h>
#include <thread>

PyObject* PyInit_Millennium(void) 
{
    static struct PyModuleDef module_def = 
    { 
        PyModuleDef_HEAD_INIT, "Millennium", NULL, -1, (PyMethodDef*)GetMillenniumModule() 
    };

    return PyModule_Create(&module_def);
}

const std::string GetPythonVersion()
{
    PyThreadState* mainThreadState = PyThreadState_New(PyInterpreterState_Main());
    PyEval_RestoreThread(mainThreadState);
    PyThreadState* newInterp = Py_NewInterpreter();
    std::string pythonVersion;

    // Run the code and get the result
    PyObject* platformModule = PyImport_ImportModule("platform");
    if (platformModule) 
    {
        PyObject* versionFn = PyObject_GetAttrString(platformModule, "python_version");
        if (versionFn && PyCallable_Check(versionFn)) 
        {
            PyObject* version_result = PyObject_CallObject(versionFn, nullptr);
            if (version_result) 
            {
                pythonVersion = PyUnicode_AsUTF8(version_result);
                Py_DECREF(version_result);
            }
            Py_DECREF(versionFn);
        }
        Py_DECREF(platformModule);
    }

    Py_EndInterpreter(newInterp); 
    PyThreadState_Clear(mainThreadState);
    PyThreadState_Swap(mainThreadState);
    PyThreadState_DeleteCurrent();

    return pythonVersion;
}

PythonManager::PythonManager() : m_InterpretorThreadSave(nullptr)
{
    Logger.LogHead("init plugin manager:");
    this->m_instanceCount = 0;

    // initialize global modules
    Logger.LogItem("hook", "locking standard output..."); PyImport_AppendInittab("hook_stdout", &PyInit_CustomStdout);
    Logger.LogItem("hook", "locking standard error...");  PyImport_AppendInittab("hook_stderr", &PyInit_CustomStderr);
    Logger.LogItem("hook", "inserting Millennium...");    PyImport_AppendInittab("Millennium", &PyInit_Millennium);

    Logger.LogItem("status", "done appending init tabs!");
    Logger.LogItem("python", "initializing python...");

    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    /* Read all configuration at once */
    status = PyConfig_Read(&config);

    if (PyStatus_Exception(status)) {
        Logger.Error("couldn't read config {}", status.err_msg);
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
        Logger.Error("couldn't initialize from config {}", status.err_msg);
        goto done;
    }

    m_InterpretorThreadSave = PyEval_SaveThread();
done:
    PyConfig_Clear(&config);

    const std::string version = GetPythonVersion();
    Logger.LogItem("python", fmt::format("initialized python {}", version));

    if (version != "3.11.8") {
        Logger.Warn("Millennium is intended to run python 3.11.8. You may be prone to stability issues...");
    }
}

PythonManager::~PythonManager()
{
    Logger.Error("PythonManager was destroyed.");
    PyEval_RestoreThread(m_InterpretorThreadSave);
    // Py_FinalizeEx();
}

bool PythonManager::CreatePythonInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema&)> callback)
{
    const std::string pluginName = plugin.pluginName;
    this->m_instanceCount++;

    auto thread = std::thread([this, pluginName, callback, &plugin] 
    {
        PyThreadState* threadStateMain = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(threadStateMain);
        PyThreadState* interpreterState = Py_NewInterpreter();

        PyThreadState_Swap(interpreterState);
        {
            this->m_pythonInstances.push_back({ pluginName, interpreterState });
            RedirectOutput();
            callback(plugin);
        }
        //Py_EndInterpreter(subts); closes intepretor

        PyThreadState_Clear(threadStateMain);
        PyThreadState_Swap(threadStateMain);
        PyThreadState_DeleteCurrent();
    });

    this->m_threadPool.push_back(std::move(thread));
    return true;
}

bool PythonManager::ShutdownPlugin(std::string plugin_name)
{
    bool success = false;

    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); ++it) 
    {
        const auto& [pluginName, thread_ptr] = *it;

        if (pluginName != plugin_name) 
        {
            continue;
        }
        
        success = true;

        if (thread_ptr == nullptr) 
        {
            Logger.Error(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
            success = false;
        }

        PyThreadState* threadStateMain = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(threadStateMain);
        PyGILState_STATE GILState = PyGILState_Ensure();
        PyThreadState_Swap(thread_ptr);

        if (thread_ptr == NULL) 
        {
            Logger.Error("script execution was queried but the receiving parties thread state was nullptr");
            success = false;
        }

        if (PyRun_SimpleString("plugin._unload()") != 0) 
        {
            PyErr_Print();
            Logger.Error("millennium failed to shutdown [{}]", plugin_name);
            success = false;
        }

        if (PyRun_SimpleString("import sys\nsys.exit()") != 0)
        {
            PyErr_Print();
            Logger.Error("millennium failed to shutdown [{}]", plugin_name);
            success = false;
        }

        // close the python interpretor. GIL must be held and the interpretor must be IDLE
        Py_EndInterpreter(thread_ptr);
        PyThreadState_Clear(threadStateMain);
        PyThreadState_Swap(threadStateMain);
        PyGILState_Release(GILState);
        PyThreadState_DeleteCurrent();

        this->m_pythonInstances.erase(it);
        break;
    }

    Logger.Log("Successfully shut down [{}].\nRemaining: ", plugin_name);
    
    for (const auto& plugin: this->m_pythonInstances) 
    {
        Logger.Log(plugin.pluginName);
    }

    return true;
}

void PythonManager::WaitForExit()
{
    for (auto& thread : this->m_threadPool) {
        thread.detach();
    }
}

PythonThreadState PythonManager::GetPythonThreadStateFromName(std::string pluginName)
{
    for (const auto& [pluginName, thread_ptr] : this->m_pythonInstances) {
        if (pluginName == pluginName) {
            return {
                pluginName, thread_ptr
            };
        }
    }
    return {};
}

std::string PythonManager::GetPluginNameFromThreadState(PyThreadState* thread) 
{
    for (const auto& [pluginName, thread_ptr] : this->m_pythonInstances) {

        if (thread_ptr == nullptr) {
            Logger.Error("thread_ptr was nullptr");
            return {};
        }

        if (thread_ptr == thread) return pluginName;  
    }
    return {};
}