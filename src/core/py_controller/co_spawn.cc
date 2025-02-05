#include "co_spawn.h"
#include <api/executor.h>
#include <core/loader.h>
#include <core/ffi/ffi.h>
#include <core/py_controller/bind_stdout.h>
#include <core/py_controller/logger.h>
#include <core/co_initialize/co_stub.h>
// #include <boxer/boxer.h>

std::string ThreadIdToString(const std::thread::id& id) {
    std::stringstream ss;
    ss << std::hash<std::thread::id>{}(id);
    return ss.str();
}

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

PythonManager::PythonManager() : m_InterpreterThreadSave(nullptr)
{
    #ifdef _WIN32
    // Set PYTHONHOME to the current directory
    _putenv_s("PYTHONHOME", (SystemIO::GetInstallPath() / "ext" / "data" / "cache").generic_string().c_str());
    #endif

    // initialize global modules
    PyImport_AppendInittab("hook_stdout", &PyInit_CustomStdout);
    PyImport_AppendInittab("hook_stderr", &PyInit_CustomStderr);
    PyImport_AppendInittab("PluginUtils", &PyInit_Logger);
    PyImport_AppendInittab("Millennium",  &PyInit_Millennium);

    Logger.Log("Verifying Python environment...");

    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    /* Read all configuration at once */
    status = PyConfig_Read(&config);

    if (PyStatus_Exception(status)) {
        LOG_ERROR("couldn't read config {}", status.err_msg);
        goto done;
    }

    config.write_bytecode = 0;
    config.module_search_paths_set = 1;

    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonPath.begin(),     pythonPath.end()    ).c_str());
    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonLibs.begin(),     pythonLibs.end()    ).c_str());
    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonUserLibs.begin(), pythonUserLibs.end()).c_str());

    status = Py_InitializeFromConfig(&config);

    if (PyStatus_Exception(status)) {
        LOG_ERROR("couldn't initialize from config {}", status.err_msg);
        goto done;
    }

    m_InterpreterThreadSave = PyEval_SaveThread();
done:
    PyConfig_Clear(&config);

    const std::string version = GetPythonVersion();

    if (version != "3.11.8") {
        Logger.Warn("Millennium is intended to run python 3.11.8. You may be prone to stability issues...");
    }
}

PythonManager::~PythonManager()
{
    Logger.Warn("Deconstructing {} plugin(s) and preparing for exit...", this->m_pythonInstances.size());
    
    for (const auto& [pluginName, threadState, interpMutex] : this->m_pythonInstances) 
    {
        Logger.Warn("Shutting down plugin '{}'", pluginName);
        this->DestroyPythonInstance(pluginName);
    }

    Logger.Log("All plugins have been shut down...");

    PyEval_RestoreThread(m_InterpreterThreadSave);
    Py_FinalizeEx();

    Logger.Log("Python has been finalized...");
}

bool PythonManager::DestroyPythonInstance(std::string plugin_name)
{
    bool successfulShutdown = false;

    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); ++it) 
    {
        const auto& [pluginName, threadState, interpMutex] = *it;

        if (pluginName != plugin_name) 
        {
            continue;
        }

        // Lock mutex in new scope to avoid deadlock
        {
            std::lock_guard<std::mutex> lg(interpMutex->mtx);
            interpMutex->flag.store(true);  // Set the flag
        }
        interpMutex->cv.notify_one(); // Notify waiting thread

        Logger.Log("Notified plugin [{}] to shut down...", plugin_name);

        for (auto it = this->m_threadPool.begin(); it != this->m_threadPool.end(); ++it) 
        {
            auto& [pluginName, thread] = *it;

            if (pluginName == plugin_name) 
            {
                Logger.Log("Trying to join thread {}...", ThreadIdToString(thread.get_id()));
                thread.join();

                Logger.Log("Successfully joined thread");
                this->m_threadPool.erase(it);
                
                CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
                backendHandler.BackendUnLoaded({ plugin_name });
                break;
            }
        }

        this->m_pythonInstances.erase(it);
        break;
    }

    return successfulShutdown;
}

bool PythonManager::IsRunning(std::string targetPluginName)
{
    for (const auto& [pluginName, threadState, interpMutex] : this->m_pythonInstances) 
    {
        if (targetPluginName == pluginName) 
        {
            return true;
        }
    }
    return false;
}

bool PythonManager::CreatePythonInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema)> callback)
{
    const std::string pluginName = plugin.pluginName;
    auto interpMutexState = std::make_shared<InterpreterMutex>();

    auto thread = std::thread([this, pluginName, callback, plugin, interpMutexStatePtr = interpMutexState ] 
    {
        PyThreadState* threadStateMain = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(threadStateMain);

        PyThreadState* interpreterState = Py_NewInterpreter();

        PyThreadState_Swap(interpreterState);
        
        this->m_pythonInstances.push_back({ pluginName, interpreterState, interpMutexStatePtr });
        RedirectOutput();
        callback(plugin);
        
        PyThreadState_Clear(threadStateMain);
        PyThreadState_Swap(threadStateMain);
        PyThreadState_DeleteCurrent();

        // Sit on the mutex until daddy says it's time to go
        std::unique_lock<std::mutex> lock(interpMutexStatePtr->mtx);
        interpMutexStatePtr->cv.wait(lock, [interpMutexStatePtr] { 
            return interpMutexStatePtr->flag.load();
        });

        Logger.Log("Orphaned '{}', jumping off the mutex lock...", pluginName);
        
        std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
        pythonGilLock->HoldAndLockGILOnThread(interpreterState);

        if (pluginName != "pipx" && PyRun_SimpleString("plugin._unload()") != 0) 
        {
            PyErr_Print();
            Logger.Warn("'{}' refused to shutdown properly, force shutting down plugin...", pluginName);
            ErrorToLogger(pluginName, "Failed to shut down plugin properly, force shutting down plugin...");
        }

        Logger.Log("Shutting down plugin '{}'", pluginName);
        Py_EndInterpreter(interpreterState);
        Logger.Log("Ended sub-interpreter...", pluginName);
        pythonGilLock->ReleaseAndUnLockGIL();
        Logger.Log("Shut down plugin '{}'", pluginName);
    });

    this->m_threadPool.push_back({ pluginName, std::move(thread) });
    return true;
}

PythonThreadState PythonManager::GetPythonThreadStateFromName(std::string targetPluginName)
{
    for (const auto& [pluginName, thread_ptr, interpMutex] : this->m_pythonInstances) 
    {
        if (targetPluginName == pluginName) 
        {
            return { pluginName, thread_ptr };
        }
    }
    return {};
}

std::string PythonManager::GetPluginNameFromThreadState(PyThreadState* thread) 
{
    for (const auto& [pluginName, thread_ptr, interpMutex] : this->m_pythonInstances)
    {
        if (thread_ptr != nullptr && thread_ptr == thread) 
        {
            return pluginName;
        }
    }
    return {};
}