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
#include "locals.h"
#include <vector>
#include <Python.h>
#include "loader.h"

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

			bool operator==(const PluginTypeSchema& other) const 
			{
				return pluginName == other.pluginName && event == other.event;
			}
		};

		using EventCallback = std::function<void()>;

		void RegisterForLoad(EventCallback callback);
		void StatusDispatch();
		void BackendLoaded(PluginTypeSchema plugin);
		void BackendUnLoaded(PluginTypeSchema plugin, bool isShuttingDown);
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

	const void InjectFrontendShims(bool reloadFrontend = true);
	const void ReInjectFrontendShims(std::shared_ptr<PluginLoader> pluginLoader, bool reloadFrontend = true);
	const void BackendStartCallback(SettingsStore::PluginTypeSchema plugin);
}

const void SetPluginSecretName(PyObject* globalDictionary, const std::string& pluginName);