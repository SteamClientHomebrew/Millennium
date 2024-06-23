#pragma once
#include <sys/locals.h>

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

		enum eEvents 
		{
			CB_BACKENDS_READY
		};

		using EventCallback = std::function<void()>;

		void Listen(const eEvents event_name, EventCallback callback);
		void Emit(const eEvents event_name);

	private:
		BackendCallbacks() {}
		~BackendCallbacks() {}

		std::unordered_map<eEvents, std::vector<EventCallback>> listeners;
	};

	const void InjectFrontendShims(uint16_t ftpPort = 0, uint16_t ipcPort = 0);
	const void ReInjectFrontendShims(void);
	const void BackendStartCallback(SettingsStore::PluginTypeSchema plugin);
}