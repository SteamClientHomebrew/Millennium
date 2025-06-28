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

#pragma once
#include <vector>
#include <Python.h>
#include <thread>
#include <functional>
#include <string>
#include "locals.h"
#include <condition_variable>
#include <atomic>
#include "internal_logger.h"
#include <filesystem>
#include "env.h"
#include <optional>

struct InterpreterMutex {
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> flag {false};
};

struct PythonThreadState {
	std::string pluginName;
	PyThreadState* thread_state;
	std::shared_ptr<InterpreterMutex> mutex;

	PythonThreadState(std::string pluginName, PyThreadState* thread_state, std::shared_ptr<InterpreterMutex> mutex) : pluginName(pluginName), thread_state(thread_state), mutex(mutex) {}
};

static const std::filesystem::path pythonModulesBaseDir = std::filesystem::path(GetEnv("MILLENNIUM__PYTHON_ENV"));

#ifdef _WIN32
static const std::string pythonPath     = pythonModulesBaseDir.generic_string();
static const std::string pythonLibs     = (pythonModulesBaseDir / "python311.zip").generic_string();
static const std::string pythonUserLibs = (pythonModulesBaseDir / "Lib" / "site-packages").generic_string();
#elif __linux__
static const std::string pythonPath     = GetEnv("LIBPYTHON_BUILTIN_MODULES_PATH");
static const std::string pythonLibs     = GetEnv("LIBPYTHON_BUILTIN_MODULES_DLL_PATH");
static const std::string pythonUserLibs = (pythonModulesBaseDir / "lib" / "python3.11" / "site-packages").generic_string();
#elif __APPLE__
static const std::string pythonPath     = GetEnv("LIBPYTHON_BUILTIN_MODULES_PATH");
static const std::string pythonLibs     = GetEnv("LIBPYTHON_BUILTIN_MODULES_DLL_PATH");
static const std::string pythonUserLibs = (pythonModulesBaseDir / "lib" / "python3.11" / "site-packages").generic_string();
#endif

class PythonManager 
{
private:
	std::mutex m_pythonMutex;
	PyThreadState* m_InterpreterThreadSave;

	std::vector<std::tuple<std::string, std::thread>> m_threadPool;
	std::vector<std::shared_ptr<PythonThreadState>> m_pythonInstances;

public:
	PythonManager();
	~PythonManager();

	bool DestroyPythonInstance(std::string targetPluginName, bool isShuttingDown = false);
	bool DestroyAllPythonInstances();
	bool CreatePythonInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema)> callback);

	bool IsRunning(std::string pluginName);
	bool HasBackend(std::string pluginName);

	std::optional<std::shared_ptr<PythonThreadState>> GetPythonThreadStateFromName(std::string pluginName);
	std::string GetPluginNameFromThreadState(PyThreadState* thread);

	static PythonManager& GetInstance() {
		static PythonManager InstanceRef;
		return InstanceRef;
	}
};