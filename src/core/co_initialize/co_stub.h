#pragma once
#include <sys/locals.h>
#include <vector>
#include <Python.h>

template <typename T>
class Singleton 
{
public:
    static T& getInstance() 
	{
        static T instance;
        return instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton() {}
    virtual ~Singleton() {}
};

namespace CoInitializer
{
	class BackendCallbacks : public Singleton<BackendCallbacks> 
	{
		friend class Singleton<BackendCallbacks>;
	public:

		enum eBackendLoadEvents 
		{
			BACKEND_LOAD_SUCCESS,
			BACKEND_LOAD_FAILED,
		};

		struct PluginTypeSchema 
		{
			std::string pluginName;
			eBackendLoadEvents event;
		};

		using EventCallback = std::function<void()>;

		void RegisterForLoad(EventCallback callback);
		void StatusDispatch();
		void BackendLoaded(PluginTypeSchema plugin);
		void BackendUnLoaded(PluginTypeSchema plugin);
		void Reset();

	private:
		BackendCallbacks() {}
		~BackendCallbacks() {}

		bool EvaluateBackendStatus();
		std::string GetFailedBackendsStr();
		std::string GetSuccessfulBackendsStr();

		enum eEvents 
		{
			ON_BACKEND_READY_EVENT
		};

		bool isReadyForCallback = false;
		std::vector<PluginTypeSchema> emittedPlugins;
		std::vector<eEvents> missedEvents;
		std::unordered_map<eEvents, std::vector<EventCallback>> listeners;
	};

	const void InjectFrontendShims(uint16_t ftpPort = 0, uint16_t ipcPort = 0);
	const void ReInjectFrontendShims(void);
	const void BackendStartCallback(SettingsStore::PluginTypeSchema plugin);
}

const void SetPluginSecretName(PyObject* globalDictionary, const std::string& pluginName);