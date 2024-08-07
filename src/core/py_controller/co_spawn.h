#pragma once
#include <vector>
#include <Python.h>
#include <thread>
#include <functional>
#include <string>
#include <sys/locals.h>
#include <sys/log.h>
#include <filesystem>

struct PythonThreadState {
	std::string pluginName;
	PyThreadState* thread_state;
};

static const std::filesystem::path pythonModulesBaseDir = SystemIO::GetInstallPath() / "ext" / "data" / "cache";

#ifdef _WIN32
static const std::string pythonPath = pythonModulesBaseDir.generic_string();
static const std::string pythonLibs = (pythonModulesBaseDir / "python311.zip").generic_string();
static const std::string pythonUserLibs = (pythonModulesBaseDir / "Lib" / "site-packages").generic_string();
#elif __linux__
static const std::string pythonPath = (pythonModulesBaseDir / "lib" / "python3.11").generic_string();
static const std::string pythonLibs = (pythonModulesBaseDir / "lib" / "python3.11" / "lib-dynload").generic_string();
static const std::string pythonUserLibs = (pythonModulesBaseDir / "lib" / "python3.11" / "site-packages").generic_string();
#endif


class PythonManager 
{
private:
	PyThreadState* m_InterpreterThreadSave;
	short m_instanceCount;

	std::vector<std::thread> m_threadPool;
	std::vector<PythonThreadState> m_pythonInstances;

public:
	PythonManager();
	~PythonManager();

	bool RemovePythonInstance(std::string plugin_name);
	bool CreatePythonInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema)> callback);
	void WaitForExit();

	PythonThreadState GetPythonThreadStateFromName(std::string pluginName);
	std::string GetPluginNameFromThreadState(PyThreadState* thread);

	bool ShutdownPlugin(std::string plugin_name);

	static PythonManager& GetInstance() {
		static PythonManager InstanceRef;
		return InstanceRef;
	}
};