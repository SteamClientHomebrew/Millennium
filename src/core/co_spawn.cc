/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "co_spawn.h"
#include "executor.h"
#include "loader.h"
#include "ffi.h"
#include "bind_stdout.h"
#include "logger.h"
#include "co_stub.h"

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

    if (PyStatus_Exception(status)) 
    {
        LOG_ERROR("couldn't read config {}", status.err_msg);
        goto done;
    }

    PyConfig_SetString(&config, &config.home, std::wstring(pythonPath.begin(), pythonPath.end()).c_str());
    config.write_bytecode = 0;
    config.module_search_paths_set = 1;

    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonPath.begin(),     pythonPath.end()    ).c_str());
    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonLibs.begin(),     pythonLibs.end()    ).c_str());
    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonUserLibs.begin(), pythonUserLibs.end()).c_str());

    status = Py_InitializeFromConfig(&config);

    if (PyStatus_Exception(status)) 
    {
        LOG_ERROR("couldn't initialize from config {}", status.err_msg);
        goto done;
    }

    m_InterpreterThreadSave = PyEval_SaveThread();
done:
    PyConfig_Clear(&config);

    const std::string version = GetPythonVersion();

    if (version != "3.11.8") 
    {
        Logger.Warn("Millennium is intended to run python 3.11.8. You may be prone to stability issues...");
    }
}

PythonManager::~PythonManager()
{
    Logger.Warn("Deconstructing {} plugin(s) and preparing for exit...", this->m_pythonInstances.size());
    
    this->DestroyAllPythonInstances();

    Logger.Log("All plugins have been shut down...");

    PyEval_RestoreThread(m_InterpreterThreadSave);
    Py_FinalizeEx();

    for (auto& [pluginName, thread] : m_threadPool) 
    {
        Logger.Log("Joining thread for plugin '{}'", pluginName);
        thread.join();
    }

    Logger.Log("Finished shutdown! Bye bye!");
}

bool PythonManager::DestroyAllPythonInstances()
{
    const auto startTime = std::chrono::steady_clock::now();

    std::atomic<bool> timeOutLockThreadRunning = true;
    std::unique_lock<std::mutex> lock(this->m_pythonMutex);  // Lock for thread safety

    std::thread timeOutThread([&timeOutLockThreadRunning, startTime] 
    {
        while (timeOutLockThreadRunning.load()) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(10)) 
            {
                LOG_ERROR("Exceeded 10 second timeout for shutting down plugins, force terminating Steam. This is likely a plugin issue, unrelated to Millennium.");
                
                #ifdef _WIN32
                std::exit(1);
                #elif __linux__
                raise(SIGINT);
                #endif
            }
        }
    });

    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); /* No increment */) 
    {
        auto& [pluginName, threadState, interpMutex] = *(*it);

        Logger.Log("Instance state: {}", static_cast<void*>(&(*it)));
        {
            std::lock_guard<std::mutex> lg(interpMutex->mtx); 
            interpMutex->flag.store(true); 
            interpMutex->cv.notify_all();
        }
        Logger.Log("Notified plugin [{}] to shut down...", pluginName);

        // Join and remove the corresponding thread safely
        auto threadIt = std::find_if(this->m_threadPool.begin(), this->m_threadPool.end(), [&](const auto& t) { return std::get<0>(t) == pluginName; }); 

        if (threadIt != this->m_threadPool.end()) 
        {
            auto& [threadPluginName, thread] = *threadIt;

            Logger.Log("Joining thread for plugin '{}'", threadPluginName);
            thread.join(); 
            Logger.Log("Successfully joined thread");

            this->m_threadPool.erase(threadIt);
            CoInitializer::BackendCallbacks::getInstance().BackendUnLoaded({ pluginName }, true);
        }
        else
        {
            LOG_ERROR("Couldn't find thread for plugin '{}'", pluginName);
        }

        it = this->m_pythonInstances.erase(it);
        Logger.Log("New iterator position after erase: {}", static_cast<void*>(&(*it)));
    }

    timeOutLockThreadRunning.store(false);
    timeOutThread.join();  
    return true;
}
bool PythonManager::DestroyPythonInstance(std::string targetPluginName, bool isShuttingDown)
{
    bool successfulShutdown = false;

    std::unique_lock<std::mutex> lock(this->m_pythonMutex);  // Lock for thread safety

    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); /* No increment */) 
    {
        auto& [pluginName, threadState, interpMutex] = *(*it);

        if (pluginName != targetPluginName) 
        {
            ++it;
            continue;
        }

        Logger.Log("Instance state: {}", (void*)&it);

        {
            std::lock_guard<std::mutex> lg(interpMutex->mtx); 
            interpMutex->flag.store(true); 
            interpMutex->cv.notify_all();
        }

        Logger.Log("Notified plugin [{}] to shut down...", targetPluginName);

        // Remove from thread pool safely
        for (auto threadIt = this->m_threadPool.begin(); threadIt != this->m_threadPool.end(); /* No increment */) 
        {
            auto& [threadPluginName, thread] = *threadIt;

            if (threadPluginName == targetPluginName) 
            {
                Logger.Log("Joining thread for plugin '{}'", targetPluginName);
                thread.join();

                Logger.Log("Successfully joined thread");
                threadIt = this->m_threadPool.erase(threadIt);  // Safe erase
                CoInitializer::BackendCallbacks::getInstance().BackendUnLoaded({ targetPluginName }, isShuttingDown);
                break;
            } 
            else 
            {
                ++threadIt;
            }
        }

        it = this->m_pythonInstances.erase(it);  // Safe erase        
        successfulShutdown = true;
        break;
    }

    Logger.Log("Length of python instances: {}", this->m_pythonInstances.size());
    return successfulShutdown;
}

bool PythonManager::IsRunning(std::string targetPluginName)
{
    for (auto instance : this->m_pythonInstances) 
    {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

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
    std::shared_ptr<InterpreterMutex> interpMutexState = std::make_shared<InterpreterMutex>();

    auto thread = std::thread([this, pluginName, callback, plugin, interpMutexStatePtr = interpMutexState ] 
    {
        PyThreadState* threadStateMain = PyThreadState_New(PyInterpreterState_Main());
        PyEval_RestoreThread(threadStateMain);

        PyThreadState* interpreterState = Py_NewInterpreter();
        PyThreadState_Swap(interpreterState);
        
        std::shared_ptr<PythonThreadState> threadState = std::make_shared<PythonThreadState>(std::string(pluginName), interpreterState, interpMutexStatePtr);

        this->m_pythonInstances.push_back(threadState);
        RedirectOutput();
        callback(plugin);

        Logger.Log("Plugin '{}' finished delegating callback function...", pluginName);
        
        PyThreadState_Clear(threadStateMain);
        PyThreadState_Swap(threadStateMain);
        PyThreadState_DeleteCurrent();

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

std::shared_ptr<PythonThreadState> PythonManager::GetPythonThreadStateFromName(std::string targetPluginName)
{
    for (auto instance : this->m_pythonInstances) 
    {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

        if (targetPluginName == pluginName) 
        {
            return instance;
        }
    }
    return {};
}

std::string PythonManager::GetPluginNameFromThreadState(PyThreadState* thread) 
{
    for (auto instance : this->m_pythonInstances)
    {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

        if (thread_ptr != nullptr && thread_ptr == thread) 
        {
            return pluginName;
        }
    }
    return {};
}