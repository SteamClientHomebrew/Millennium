#pragma once
#include <vector>
#include <Python.h>
#include <thread>
#include <functional>
#include <string>
#include <locals/locals.h>
#include <utilities/log.hpp>

struct PythonThreadState {
	std::string pluginName;
	PyThreadState* thread_state;
};

class PythonManager 
{
private:
	PyThreadState* m_InterpretorThreadSave;
	short m_instanceCount;

	std::vector<std::thread> m_threadPool;
	std::vector<PythonThreadState> m_pythonInstances;

public:
	PythonManager();
	~PythonManager();

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