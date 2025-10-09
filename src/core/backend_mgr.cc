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

#include "millennium/backend_mgr.h"
#include "millennium/backend_init.h"
#include "millennium/ffi.h"
#include "millennium/init.h"
#include "millennium/libpy_stdout_fwd.h"
#include "millennium/plugin_api_init.h"
#include "millennium/plugin_logger.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <optional>

std::unordered_map<std::string, std::atomic<size_t>> BackendManager::sPluginMemoryUsage;
std::mutex BackendManager::sPluginMapMutex;
std::atomic<size_t> BackendManager::sTotalAllocated{ 0 };

extern std::condition_variable cv_hasAllPythonPluginsShutdown, cv_hasSteamUnloaded;
extern std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded;

/**
 * @brief Initializes the Millennium module.
 * This function initializes the Millennium module.
 *
 * @returns {PyObject*} - A pointer to the Millennium module.
 */
PyObject* PyInit_Millennium(void)
{
    static struct PyModuleDef module_def = { PyModuleDef_HEAD_INIT, "Millennium", NULL, -1, (PyMethodDef*)PyGetMillenniumModule() };

    return PyModule_Create(&module_def);
}

/**
 * @brief Gets the Python version.
 * This function gets the Python version.
 *
 * @returns {std::string} - The Python version in string format.
 */
const std::string GetPythonVersion()
{
    PyThreadState* mainThreadState = PyThreadState_New(PyInterpreterState_Main());
    PyEval_RestoreThread(mainThreadState);
    PyThreadState* newInterp = Py_NewInterpreter();
    std::string pythonVersion;

    // Run the code and get the result
    PyObject* platformModule = PyImport_ImportModule("platform");
    if (platformModule) {
        PyObject* versionFn = PyObject_GetAttrString(platformModule, "python_version");
        if (versionFn && PyCallable_Check(versionFn)) {
            PyObject* version_result = PyObject_CallObject(versionFn, nullptr);
            if (version_result) {
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

PythonEnvPath GetPythonEnvPaths()
{
    static const std::filesystem::path pythonModulesBaseDir = std::filesystem::path(GetEnv("MILLENNIUM__PYTHON_ENV"));

#ifdef _WIN32
    static const std::string pythonPath = pythonModulesBaseDir.generic_string();
    static const std::string pythonLibs = (pythonModulesBaseDir / "python311.zip").generic_string();
    static const std::string pythonUserLibs = (pythonModulesBaseDir / "Lib" / "site-packages").generic_string();
#elif __linux__
    static const std::string pythonPath = GetEnv("LIBPYTHON_BUILTIN_MODULES_PATH");
    static const std::string pythonLibs = GetEnv("LIBPYTHON_BUILTIN_MODULES_DLL_PATH");
    static const std::string pythonUserLibs = (pythonModulesBaseDir / "lib" / "python3.11" / "site-packages").generic_string();
#elif __APPLE__
    static const std::string pythonPath = GetEnv("LIBPYTHON_BUILTIN_MODULES_PATH");
    static const std::string pythonLibs = GetEnv("LIBPYTHON_BUILTIN_MODULES_DLL_PATH");
    static const std::string pythonUserLibs = (pythonModulesBaseDir / "lib" / "python3.11" / "site-packages").generic_string();
#endif

    return { pythonPath, pythonLibs, pythonUserLibs };
}

BackendManager::~BackendManager()
{
/**
 * deconstructor's aren't used on windows as the dll loader lock causes dead locks.
 * we free from the main function instead
 * */
#ifdef __linux__
    this->Shutdown();
#endif
}

/**
 * @brief Constructor for the BackendManager class.
 *
 * This constructor initializes the Python interpreter and global modules.
 * - It hooks the stdout and stderr to the Millennium logger.
 * - It initializes the global modules.
 * - It verifies the Python environment.
 * - It initializes the Python interpreter from the config.
 * - It sets the Python version to 3.11.8.
 *
 * @returns {BackendManager} - A new BackendManager instance.
 */
BackendManager::BackendManager() : m_InterpreterThreadSave(nullptr)
{
    const auto [pythonPath, pythonLibs, pythonUserLibs] = GetPythonEnvPaths();

    // initialize global modules
    PyImport_AppendInittab("hook_stdout", &PyInit_CustomStdout);
    PyImport_AppendInittab("hook_stderr", &PyInit_CustomStderr);
    PyImport_AppendInittab("PluginUtils", &PyInit_Logger);
    PyImport_AppendInittab("Millennium", &PyInit_Millennium);

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

    PyConfig_SetString(&config, &config.home, std::wstring(pythonPath.begin(), pythonPath.end()).c_str());
    config.write_bytecode = 0;
    config.module_search_paths_set = 1;

    Logger.Log("Using python home: {}", pythonPath);
    Logger.Log("Using python libs: {}", pythonLibs);
    Logger.Log("Using python user libs: {}", pythonUserLibs);

    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonPath.begin(), pythonPath.end()).c_str());
    PyWideStringList_Append(&config.module_search_paths, std::wstring(pythonLibs.begin(), pythonLibs.end()).c_str());
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

/**
 * @brief Destroys all Python instances.
 *
 * This function destroys all Python instances and removes them from the list of Python instances.
 */
void BackendManager::Shutdown()
{
    Logger.Warn("Unloading {} plugin(s) and preparing for exit...", this->m_pythonInstances.size() + this->m_luaThreadPool.size());

    const auto startTime = std::chrono::steady_clock::now();
    // TODO: Since we have the loader locked here, we can't spawn a thread to monitor the shutdown process.
    //     std::atomic<bool> timeOutLockThreadRunning = true;

    //     std::thread timeOutThread([&timeOutLockThreadRunning, startTime]
    //     {
    //         Logger.Log("Starting 10 second timeout thread for plugin shutdown...");

    //         while (timeOutLockThreadRunning.load()) {
    //             std::this_thread::sleep_for(std::chrono::milliseconds(10));

    //             Logger.Log("Time elapsed: {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());

    //             if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(10)) {
    //                 LOG_ERROR("Exceeded 10 second timeout for shutting down plugins, force terminating Steam. This is likely a plugin issue, unrelated to Millennium.");

    //                 __debugbreak();
    // #ifdef _WIN32
    //                 std::exit(1);
    // #elif __linux__
    //                 raise(SIGINT);
    // #endif
    //             }
    //         }
    //     });

    Logger.Log("[BackendManager::Shutdown] Destroying all Lua instances...");
    this->ShutdownLuaBackends();
    Logger.Log("[BackendManager::Shutdown] Destroying all plugin instances...");
    this->ShutdownPythonBackends();

    Logger.Log("[BackendManager::Shutdown] All plugins have been shut down...");

    PyEval_RestoreThread(m_InterpreterThreadSave);
    Py_FinalizeEx();

    /** Shutdown Python interpreters */
    for (auto& [pluginName, thread] : m_pyThreadPool) {
        Logger.Log("Joining thread for plugin '{}'", pluginName);
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_pyThreadPool.clear();

    /** Shutdown Lua interpreters */
    for (auto& [pluginName, thread, L, hasFinished] : m_luaThreadPool) {
        Logger.Log("Joining Lua thread for plugin '{}'", pluginName);
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_luaThreadPool.clear();

    Logger.Log("Finished shutdown! Bye bye!");

    // timeOutLockThreadRunning.store(false);
    // timeOutThread.join();

    Logger.Log("Shutdown took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());
}

/**
 * @brief Check if any Python backends are running.
 */
bool BackendManager::HasAnyPythonBackends()
{
    return this->m_pyThreadPool.size() > 0;
}

/**
 * @brief Check if any Lua backends are running.
 */
bool BackendManager::HasAnyLuaBackends()
{
    return this->m_luaThreadPool.size() > 0;
}

/**
 * @brief Destroys all Lua instances.
 * This function destroys all Lua instances and removes them from the list of Lua instances.
 */
bool BackendManager::ShutdownLuaBackends()
{
    /** No lua instances to shutdown */
    if (m_luaThreadPool.empty()) {
        Logger.Log("No Lua instances to destroy.");
        return true;
    }

    /** create list of all plugin names to shutdown (to prevent access violations when destroying instances) */
    std::vector<std::string> pluginNames;
    for (auto& [pluginName, thread, L, hasFinished] : m_luaThreadPool) {
        pluginNames.push_back(pluginName);
    }

    /** Notify all plugins to shutdown */
    for (auto& pluginName : pluginNames) {
        Logger.Log("[Lua] Shutting down plugin [{}]", pluginName);
        DestroyLuaInstance(pluginName);
    }

    return true;
}

/**
 * @brief Destroys all Python instances.
 *
 * This function destroys all Python instances and removes them from the list of Python instances.
 *
 * @returns {bool} - True if the Python instances were destroyed successfully, false otherwise.
 */
bool BackendManager::ShutdownPythonBackends()
{
    std::unique_lock<std::mutex> lock(this->m_pythonMutex); // Lock for thread safety

    /** No python instances to shutdown */
    if (m_pythonInstances.empty()) {
        Logger.Log("No Python instances to destroy.");
        return true;
    }

    /** Notify all plugins to shutdown */
    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); it++) {
        auto& [pluginName, threadState, interpMutex] = *(*it);

        std::lock_guard<std::mutex> lg(interpMutex->mtx);
        interpMutex->flag.store(true);
        interpMutex->cv.notify_all();

        Logger.Log("Notified plugin [{}] to shut down...", pluginName);
    }

    std::unique_lock<std::mutex> lk(mtx_hasAllPythonPluginsShutdown);
    cv_hasAllPythonPluginsShutdown.wait(lk);

    Logger.Log("All plugins have acknowledged shutdown, joining threads...");

    /** Join all Python threads after they've been shutdown */
    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); /* No increment */) {
        auto& [pluginName, threadState, interpMutex] = *(*it);

        // Join and remove the corresponding thread safely
        auto threadIt = std::find_if(this->m_pyThreadPool.begin(), this->m_pyThreadPool.end(), [pluginName = pluginName](const auto& t) { return std::get<0>(t) == pluginName; });

        if (threadIt != this->m_pyThreadPool.end()) {
            auto& [threadPluginName, thread] = *threadIt;

            Logger.Log("Joining thread for plugin '{}'", pluginName);
            if (thread.joinable()) {
                thread.join();
            }
            this->m_pyThreadPool.erase(threadIt);

            CoInitializer::BackendCallbacks::getInstance().BackendUnLoaded({ pluginName }, true);
        } else {
            LOG_ERROR("Couldn't find thread for plugin '{}'", pluginName);
        }

        it = this->m_pythonInstances.erase(it);
        Logger.Log("New iterator position after erase: {}", static_cast<void*>(&(*it)));
    }

    return true;
}

/**
 * @brief Destroys a Python instance for a plugin.
 *
 * This function destroys a Python instance for a plugin and removes it from the list of Python instances.
 *
 * @param {std::string} targetPluginName - The name of the plugin to destroy the Python instance for.
 * @param {bool} isShuttingDown - Whether the plugin is shutting down.
 *
 * @returns {bool} - True if the Python instance was destroyed successfully, false otherwise.
 */
bool BackendManager::DestroyPythonInstance(std::string targetPluginName, bool isShuttingDown)
{
    bool successfulShutdown = false;

    std::unique_lock<std::mutex> lock(this->m_pythonMutex); // Lock for thread safety

    for (auto it = this->m_pythonInstances.begin(); it != this->m_pythonInstances.end(); /* No increment */) {
        auto& [pluginName, threadState, interpMutex] = *(*it);

        if (pluginName != targetPluginName) {
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
        for (auto threadIt = this->m_pyThreadPool.begin(); threadIt != this->m_pyThreadPool.end(); /* No increment */) {
            auto& [threadPluginName, thread] = *threadIt;

            if (threadPluginName == targetPluginName) {
                Logger.Log("Joining thread for plugin '{}'", targetPluginName);
                thread.join();

                Logger.Log("Successfully joined thread");
                threadIt = this->m_pyThreadPool.erase(threadIt); // Safe erase
                CoInitializer::BackendCallbacks::getInstance().BackendUnLoaded({ targetPluginName }, isShuttingDown);
                break;
            } else {
                ++threadIt;
            }
        }

        it = this->m_pythonInstances.erase(it); // Safe erase
        successfulShutdown = true;
        break;
    }

    Logger.Log("Length of python instances: {}", this->m_pythonInstances.size());
    return successfulShutdown;
}

/**
 * @brief Calls the on_unload function in the Lua plugin definition.
 * This function checks if the MILLENNIUM_PLUGIN_DEFINITION table exists and if it has an on_unload function.
 * If both exist, it calls the on_unload function and handles any errors that may occur.
 */
void BackendManager::CallLuaOnUnload(lua_State* L, const std::string& pluginName)
{
    lua_getglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1); // remove non-table
        return;
    }

    lua_getfield(L, -1, "on_unload");

    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2); // remove non-function and table
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        LOG_ERROR("Failed to unload lua plugin '{}': {}", pluginName, err ? err : "unknown error");
        lua_pop(L, 1); // remove error
    }

    lua_pop(L, 1); // remove MILLENNIUM_PLUGIN_DEFINITION table
}

/**
 * @brief Cleans up the plugin name pointer associated with a Lua state.
 */
void BackendManager::CleanupPluginNamePointer(lua_State* L)
{
    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    if (ud) {
        delete static_cast<std::shared_ptr<std::string>*>(ud);
    }
}

/**
 * @brief Finds a mutex entry for a given Lua state.
 */
void BackendManager::RemoveMutexFromPool(lua_State* L)
{
    m_luaMutexPool.erase(std::remove_if(m_luaMutexPool.begin(), m_luaMutexPool.end(), [L](const auto& entry) { return std::get<0>(entry) == L; }), m_luaMutexPool.end());
}

/**
 * @brief Removes memory tracking for a plugin.
 * This function removes memory tracking for a plugin by erasing its entry from the sPluginMemoryUsage map.
 */
void BackendManager::RemoveMemoryTracking(const std::string& pluginName)
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    sPluginMemoryUsage.erase(pluginName);
}

/**
 * @brief Destroys a Lua instance for a plugin.
 *
 * This function iterates through the list of Lua threads and destroys the instance for the specified plugin.
 */
bool BackendManager::DestroyLuaInstance(std::string pluginName, bool shouldCleanupThreadPool)
{
    for (auto it = this->m_luaThreadPool.begin(); it != this->m_luaThreadPool.end(); /* No increment */) {
        auto& [threadPluginName, thread, L, hasFinished] = *it;

        if (threadPluginName != pluginName) {
            ++it;
            continue;
        }

        Logger.Log("Joining Lua thread for plugin '{}'", pluginName);

        if (thread.joinable()) {
            thread.join();
        }

        if (!L) {
            if (shouldCleanupThreadPool) {
                it = this->m_luaThreadPool.erase(it);
            } else {
                it->hasFinished.store(true);
                {
                    std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
                    cv_hasSteamUnloaded.notify_all();
                }
                ++it;
            }
            CoInitializer::BackendCallbacks::getInstance().BackendUnLoaded({ pluginName }, false);
            return true;
        }

        Lua_LockLua(L);
        {
            CallLuaOnUnload(L, pluginName);
            CleanupPluginNamePointer(L);

            lua_close(L);
        }
        Lua_UnlockLua(L);

        RemoveMutexFromPool(L);
        RemoveMemoryTracking(pluginName);

        if (shouldCleanupThreadPool) {
            it = this->m_luaThreadPool.erase(it);
        } else {
            it->hasFinished.store(true);
            {
                std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
                cv_hasSteamUnloaded.notify_all();
            }
            ++it;
        }
        CoInitializer::BackendCallbacks::getInstance().BackendUnLoaded({ pluginName }, false);
        return true;
    }
    return false;
}

/**
 * @brief Checks if all Python backends have stopped.
 *
 * This function iterates through the list of Python instances and returns true if all backends have stopped.
 */
bool BackendManager::HasAllPythonBackendsStopped()
{
    for (auto instance : this->m_pythonInstances) {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

        if (!interpMutex->hasFinished.load()) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Checks if all Lua backends have stopped.
 *
 * This function iterates through the list of Lua threads and returns true if all backends have stopped.
 */
bool BackendManager::HasAllLuaBackendsStopped()
{
    for (auto& [pluginName, thread, L, hasFinished] : m_luaThreadPool) {
        if (!hasFinished.load()) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Creates a new Python instance for a plugin.
 *
 * This function creates a new Python instance for a plugin and delegates the callback function to the thread.
 *
 * @param {SettingsStore::PluginTypeSchema} plugin - The plugin to create a Python instance for.
 * @param {std::function<void(SettingsStore::PluginTypeSchema)>} callback - The callback function to delegate to the thread.
 *
 * @returns {bool} - True if the Python instance was created successfully, false otherwise.
 */
bool BackendManager::CreatePythonInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema)> callback)
{
    const std::string pluginName = plugin.pluginName;
    std::shared_ptr<InterpreterMutex> interpMutexState = std::make_shared<InterpreterMutex>();

    auto thread = std::thread([this, pluginName, callback, plugin, interpMutexStatePtr = interpMutexState]
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

        interpMutexStatePtr->cv.wait(lock, [interpMutexStatePtr] { return interpMutexStatePtr->flag.load(); });

        Logger.Log("Orphaned '{}', jumping off the mutex lock...", pluginName);

        std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
        pythonGilLock->HoldAndLockGILOnThread(interpreterState);

        if (pluginName != "pipx" && PyRun_SimpleString("plugin._unload()") != 0) {
            PyErr_Print();
            Logger.Warn("'{}' refused to shutdown properly, force shutting down plugin...", pluginName);
            ErrorToLogger(pluginName, "Failed to shut down plugin properly, force shutting down plugin...");
        }

        Logger.Log("Shutting down plugin '{}'", pluginName);
        Py_EndInterpreter(interpreterState);
        Logger.Log("Ended sub-interpreter...", pluginName);
        pythonGilLock->ReleaseAndUnLockGIL();
        Logger.Log("Shut down plugin '{}'", pluginName);

        {
            std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);

            interpMutexStatePtr->hasFinished.store(true);
            cv_hasSteamUnloaded.notify_all();
        }
    });

    this->m_pyThreadPool.push_back({ pluginName, std::move(thread) });
    return true;
}

/**
 * @brief Checks if a plugin is running.
 * This function iterates through the list of Python instances and returns true if the plugin is running.
 *
 * @param {std::string} targetPluginName - The name of the plugin to check if it is running.
 * @returns {bool} - True if the plugin is running, false otherwise.
 */
bool BackendManager::IsPythonBackendRunning(std::string targetPluginName)
{
    for (auto instance : this->m_pythonInstances) {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

        if (targetPluginName == pluginName) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if a Lua backend is running for a given plugin.
 * This function iterates through the list of Lua threads and returns true if the plugin is running
 *
 * @param {std::string} targetPluginName - The name of the plugin to check if it is running.
 * @returns {bool} - True if the Lua backend is running for the given plugin, false otherwise.
 */
bool BackendManager::IsLuaBackendRunning(std::string targetPluginName)
{
    for (auto& [pluginName, thread, L, hasFinished] : m_luaThreadPool) {
        if (targetPluginName == pluginName) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if a plugin has a Python backend.
 * This function checks if a plugin has a Python backend by looking it up in the settings store.
 *
 * @param {std::string} targetPluginName - The name of the plugin to check.
 * @returns {bool} - True if the plugin has a Python backend, false otherwise.
 */
bool BackendManager::HasPythonBackend(std::string targetPluginName)
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    for (const auto& plugin : settingsStore->GetEnabledBackends()) {
        if (plugin.pluginName == targetPluginName) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Gets the Python thread state from the plugin name.
 * This function iterates through the list of Python instances and returns the thread state
 * associated with the given plugin name.
 *
 * @param {std::string} targetPluginName - The name of the plugin to get the thread state from.
 * @returns {std::shared_ptr<PythonThreadState>} - The thread state associated with the given plugin name.
 */
std::optional<std::shared_ptr<PythonThreadState>> BackendManager::GetPythonThreadStateFromName(std::string targetPluginName)
{
    for (auto instance : this->m_pythonInstances) {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

        if (targetPluginName == pluginName) {
            return instance;
        }
    }
    return std::nullopt;
}

/**
 * @brief Gets the Lua thread state from the plugin name.
 * This function iterates through the list of Lua threads and returns the Lua state
 * associated with the given plugin name.
 *
 * @param {std::string} pluginName - The name of the plugin to get the Lua state from.
 * @returns {std::optional<lua_State*>} - The Lua state associated with the given plugin name, or std::nullopt if not found.
 */
std::optional<lua_State*> BackendManager::GetLuaThreadStateFromName(std::string pluginName)
{
    for (auto& [name, thread, L, hasFinished] : m_luaThreadPool) {
        if (name == pluginName) {
            return L;
        }
    }
    return std::nullopt;
}

/**
 * @brief Gets the plugin name from the thread state.
 * This function iterates through the list of Python instances and returns the plugin name
 * associated with the given thread state.
 *
 * @param {PyThreadState*} thread - The thread state to get the plugin name from.
 * @returns {std::string} - The plugin name associated with the given thread state.
 */
std::string BackendManager::GetPluginNameFromThreadState(PyThreadState* thread)
{
    for (auto instance : this->m_pythonInstances) {
        const auto& [pluginName, thread_ptr, interpMutex] = *instance;

        if (thread_ptr != nullptr && thread_ptr == thread) {
            return pluginName;
        }
    }
    return {};
}

/**
 * @brief Gets the backend type for a plugin.
 * This function retrieves the backend type (Python or Lua) for a given plugin name.
 * If the plugin has an unspecified or unknown backend type, it defaults to Python for backward compatibility.
 */
SettingsStore::PluginBackendType BackendManager::GetPluginBackendType(std::string pluginName)
{
    for (const auto& luaPlugin : this->m_luaThreadPool) {
        const auto& [luaPluginName, thread, L, hasFinished] = luaPlugin;

        if (luaPluginName == pluginName) {
            return SettingsStore::PluginBackendType::Lua;
        }
    }

    for (const auto& pyPlugin : this->m_pythonInstances) {
        const auto& [pyPluginName, thread_ptr, interpMutex] = *pyPlugin;
        if (pyPluginName == pluginName) {
            return SettingsStore::PluginBackendType::Python;
        }
    }

    return SettingsStore::PluginBackendType::Python;
}

/**
 * @brief helper fn to find a lua_State in the pool
 */
std::tuple<lua_State*, std::unique_ptr<std::mutex>>* BackendManager::Lua_FindEntry(lua_State* L)
{
    for (auto& entry : m_luaMutexPool) {
        /** get the first entry in the tuple. structured bindings causes ref issues afaik */
        if (std::get<0>(entry) == L) {
            return &entry;
        }
    }
    return nullptr;
}

/**
 * @brief Locks the mutex for a given Lua state.
 * This function locks the mutex associated with the provided Lua state.
 */
void BackendManager::Lua_LockLua(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry)
        throw std::runtime_error("lua_State not found in pool");

    std::get<1>(*entry)->lock();
}

/**
 * @brief Unlocks the mutex for a given Lua state.
 * This function unlocks the mutex associated with the provided Lua state.
 *
 * @note this assumes the mutex is already locked by the current thread.
 */
void BackendManager::Lua_UnlockLua(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry)
        throw std::runtime_error("lua_State not found in pool");

    std::get<1>(*entry)->unlock();
}

/**
 * @brief Tries to lock the mutex for a given Lua state.
 * This function attempts to lock the mutex associated with the provided Lua state.
 * If the lock is successful, it returns true. If the lock attempt fails, it returns false.
 */
bool BackendManager::Lua_TryLockLua(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry)
        throw std::runtime_error("lua_State not found in pool");

    return std::get<1>(*entry)->try_lock();
}

/**
 * @brief Checks if the mutex for a given Lua state is locked.
 *
 * This function attempts to lock the mutex associated with the provided Lua state.
 * If the lock is successful, it immediately unlocks it and returns false, indicating that the mutex was not locked.
 * If the lock attempt fails, it returns true, indicating that the mutex is currently locked.
 */
bool BackendManager::Lua_IsMutexLocked(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry)
        throw std::runtime_error("lua_State not found in pool");

    auto& m = std::get<1>(*entry);
    if (m->try_lock()) {
        m->unlock();
        return false;
    }
    return true;
}

/**
 * @brief Gets the plugin counter for a given plugin name.
 * This function returns a reference to the atomic size_t counter for the specified plugin name.
 */
std::atomic<size_t>& BackendManager::Lua_GetPluginCounter(const std::string& plugin_name)
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    return sPluginMemoryUsage[plugin_name];
}

/**
 * @brief Gets the total memory usage.
 * This function returns the total memory usage across all plugins.
 *
 * @note this is an atomic operation and is thread-safe.
 * it prob will cause a performance hit when called too often.
 */
size_t BackendManager::Lua_GetTotalMemory()
{
    return sTotalAllocated.load();
}

/**
 * @brief Gets a snapshot of memory usage for a specific plugin.
 * This function returns a snapshot of memory usage for a specific plugin.
 *
 * @param {std::string} plugin_name - The name of the plugin to get the memory usage for.
 * @returns {size_t} - The memory usage of the specified plugin.
 */
size_t BackendManager::Lua_GetPluginMemorySnapshotByName(const std::string& plugin_name)
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    auto it = sPluginMemoryUsage.find(plugin_name);
    return (it != sPluginMemoryUsage.end()) ? it->second.load() : 0;
}

/**
 * @brief Gets a snapshot of memory usage for all plugins.
 * This function returns a snapshot of memory usage for all plugins.
 *
 * @returns {std::unordered_map<std::string, size_t>} - A map of plugin names to their memory usage.
 */
std::unordered_map<std::string, size_t> BackendManager::Lua_GetAllPluginMemorySnapshot()
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    std::unordered_map<std::string, size_t> snapshot;
    for (const auto& pair : sPluginMemoryUsage) {
        snapshot[pair.first] = pair.second.load();
    }
    return snapshot;
}

/**
 * @brief Memory profiler for Lua states.
 * This function tracks memory allocations and deallocations for each Lua state,
 * associating them with their respective plugin names.
 *
 * @param {void*} ud - User data, used to carry the plugin name.
 * @param {void*} ptr - Pointer to the memory block being reallocated or freed.
 * @param {size_t} osize - Original size of the memory block.
 * @param {size_t} nsize - New size of the memory block.
 */
void* BackendManager::Lua_MemoryProfiler(void* ud, void* ptr, size_t osize, size_t nsize)
{
    /** cast the userdata back into a shared_ptr */
    const std::string& pluginName = **static_cast<std::shared_ptr<std::string>*>(ud);

    /** new size is 0, meaning free the memory */
    if (nsize == 0) {
        if (ptr) {
            sTotalAllocated.fetch_sub(osize);
            Lua_GetPluginCounter(pluginName).fetch_sub(osize);
            free(ptr);
        }
        return nullptr;
    }
    /** new size is not 0, meaning reallocate the memory */
    else {
        void* newptr = realloc(ptr, nsize);
        if (!newptr) {
            /** realloc failed, return nullptr */
            return nullptr;
        }

        auto& pluginCounter = Lua_GetPluginCounter(pluginName);

        if (ptr) {
            sTotalAllocated.fetch_sub(osize);
            pluginCounter.fetch_sub(osize);
        }

        sTotalAllocated.fetch_add(nsize);
        pluginCounter.fetch_add(nsize);
        return newptr;
    }
}

/**
 * @brief Creates a new Lua instance for a plugin.
 * This function creates a new Lua state, initializes it, and starts a new thread
 *
 * @param {SettingsStore::PluginTypeSchema} plugin - The plugin to create a Lua instance for.
 * @param {std::function<void(SettingsStore::PluginTypeSchema, lua_State*)>} callback - The callback function to delegate to the thread.
 * @returns {bool} - always true, it used to have meaning in the early days with the python backend, but has since been removed.
 */
bool BackendManager::CreateLuaInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema, lua_State*)> callback)
{
    /** create a shared_ptr for the plugin name to keep the reference alive outside of the functions scope. */
    auto pluginNamePtr = new std::shared_ptr<std::string>(std::make_shared<std::string>(plugin.pluginName));

    lua_State* L = lua_newstate(BackendManager::Lua_MemoryProfiler, pluginNamePtr);
    /** create a new "gil" for the lua state */
    m_luaMutexPool.emplace_back(L, std::make_unique<std::mutex>());

    if (!L) {
        LOG_ERROR("Failed to create new Lua state");
        return false;
    }

    /** open standard libraries */
    luaL_openlibs(L);

    std::string pluginName = plugin.pluginName;
    std::thread luaThread([L, plugin, callback]() mutable
    {
        /** call the backend manager init fn */
        if (callback)
            callback(plugin, L);
    });

    m_luaThreadPool.emplace_back(std::move(pluginName), std::move(luaThread), L);
    return true;
}
