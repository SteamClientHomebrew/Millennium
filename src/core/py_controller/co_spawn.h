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

static const std::filesystem::path pythonModulesBaseDir = SystemIO::GetSteamPath() / "ext" / "data" / "cache";

static const std::string pythonPath = pythonModulesBaseDir.generic_string();
static const std::string pythonLibs = (pythonModulesBaseDir / "python311.zip").generic_string();
static const std::string pythonUserLibs = (pythonModulesBaseDir / "Lib" / "site-packages").generic_string();

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