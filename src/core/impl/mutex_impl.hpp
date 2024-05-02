#pragma once
#include <Python.h>
#include <string>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <utilities/log.hpp>

namespace python {

	enum type_t {
		_bool,
		_string,
		_int,
		_err,
		unkown // non-atomic ADT's
	};

	struct return_t {
		std::string plain;
		type_t type;
	};

	std::string construct_fncall(nlohmann::basic_json<> data);

	return_t evaluate_lock(std::string plugin_name, std::string script);
	void run_lock(std::string plugin_name, std::string script);
}
namespace javascript {

    using EventHandler = std::function<void(const nlohmann::json& event_msg, int listenerId)>;

    class emitter {
    private:
        emitter() {}
        std::unordered_map<std::string, std::vector<std::pair<int, EventHandler>>> events;
        int nextListenerId = 0;

    public:
        emitter(const emitter&) = delete;
        emitter& operator=(const emitter&) = delete; 

        static emitter& instance() {
            static emitter instance;
            return instance;
        }

        int on(const std::string& event, EventHandler handler) {
            int listenerId = nextListenerId++;
            events[event].push_back(std::make_pair(listenerId, handler));
            return listenerId;
        }

        void off(const std::string& event, int listenerId) {
            auto it = events.find(event);
            if (it != events.end()) {
                auto& handlers = it->second;
                handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [listenerId](const auto& handler) {
                    return handler.first == listenerId;
                    }), handlers.end());
            }
        }

        void emit(const std::string& event, const nlohmann::json& data) {
            auto it = events.find(event);
            if (it != events.end()) {
                const auto& handlers = it->second;
                for (const auto& handler : handlers) {
                    handler.second(data, handler.first);
                }
            }
        }
    };

	struct param_types
	{
		std::string name, type;
	};

    const void reload_shared_context();

	const std::string construct_fncall(const char* value, const char* method_name,
		std::vector<javascript::param_types> params);

	PyObject* evaluate_lock(std::string script);
}