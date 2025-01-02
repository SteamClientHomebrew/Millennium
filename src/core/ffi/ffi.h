#pragma once
#include <Python.h>
#include <string>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <sys/log.h>
#include <thread>

class PythonGIL : public std::enable_shared_from_this<PythonGIL>
{
private:
    PyGILState_STATE m_interpreterGIL{};
    PyThreadState* m_interpreterThreadState = nullptr;
    PyInterpreterState* m_mainInterpreter = nullptr;

public:
    const void HoldAndLockGIL();
    const void HoldAndLockGILOnThread(PyThreadState* threadState);
    const void ReleaseAndUnLockGIL();

    PythonGIL();
    ~PythonGIL();
};

namespace Python {

	enum Types {
		Boolean,
		String,
		Integer,
		Error,
		Unknown // non-atomic ADT's
	};

	struct EvalResult {
		std::string plain;
		Types type;
	};

	std::string ConstructFunctionCall(nlohmann::basic_json<> data);
    std::tuple<std::string, std::string> GetExceptionInformaton();

	EvalResult LockGILAndEvaluate(std::string pluginName, std::string script);
	void LockGILAndDiscardEvaluate(std::string pluginName, std::string script);
}

namespace JavaScript {

    enum Types {
        Boolean,
        String,
        Integer
    };

    struct JsFunctionConstructTypes
    {
        std::string pluginName;
        Types type;
    };

    struct EvalResult {
        nlohmann::basic_json<> json;
        bool successfulCall;
    };

    using EventHandler = std::function<void(const nlohmann::json& eventMessage, std::string listenerId)>;

    class SharedJSMessageEmitter {
    private:
        SharedJSMessageEmitter() {}

        std::unordered_map<std::string, std::vector<std::pair<std::string, EventHandler>>> events;
        std::unordered_map<std::string, std::vector<nlohmann::json>> missedMessages; // New data structure for missed messages

    public:
        SharedJSMessageEmitter(const SharedJSMessageEmitter&) = delete;
        SharedJSMessageEmitter& operator=(const SharedJSMessageEmitter&) = delete;

        static SharedJSMessageEmitter& InstanceRef() {
            static SharedJSMessageEmitter InstanceRef;
            return InstanceRef;
        }

        std::string OnMessage(const std::string& event, const std::string name, EventHandler handler) {
            events[event].push_back(std::make_pair(name, handler));

            // Deliver any missed messages
            auto it = missedMessages.find(event);
            if (it != missedMessages.end()) {
                for (const auto message : it->second) {
                    handler(message, name);
                }
                missedMessages.erase(it); // Clear missed messages once delivered
            }
            return name;
        }

        void RemoveListener(const std::string& event, std::string listenerId) {
            auto it = events.find(event);
            if (it != events.end()) {
                auto& handlers = it->second;
                handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [listenerId](const auto& handler) {
                    if (handler.first == listenerId) {
                        return true;
                    }
                    return false;
                }), handlers.end());
            }
        }

        void EmitMessage(const std::string& event, const nlohmann::json& data) {
            auto it = events.find(event);
            if (it != events.end()) {
                const auto& handlers = it->second;
                for (const auto& handler : handlers) {
                    try {
                        handler.second(data, handler.first);
                    } catch (const std::bad_function_call& e) {
                        Logger.Warn("Failed to emit message on {}. exception: {}", handler.first, e.what());
                    }
                }
            } else {
                missedMessages[event].push_back(data);
            }
        }
    };

	const std::string ConstructFunctionCall(const char* value, const char* methodName, std::vector<JavaScript::JsFunctionConstructTypes> params);

    JavaScript::EvalResult ExecuteOnSharedJsContext(std::string javaScriptEval);
	PyObject* EvaluateFromSocket(std::string script);
}