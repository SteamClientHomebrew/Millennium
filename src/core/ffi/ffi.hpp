#pragma once
#include <Python.h>
#include <string>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <sys/log.hpp>

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

	enum ReturnTypes {
		Boolean,
		String,
		Integer,
		Error,
		Unknown // non-atomic ADT's
	};

	struct EvalResult {
		std::string plain;
		ReturnTypes type;
	};

	std::string ConstructFunctionCall(nlohmann::basic_json<> data);

	EvalResult LockGILAndEvaluate(std::string pluginName, std::string script);
	void LockGILAndDiscardEvaluate(std::string pluginName, std::string script);
}

namespace JavaScript {

    using EventHandler = std::function<void(const nlohmann::json& eventMessage, int listenerId)>;

    class SharedJSMessageEmitter {
    private:
        SharedJSMessageEmitter() {}
        std::unordered_map<std::string, std::vector<std::pair<int, EventHandler>>> events;
        int nextListenerId = 0;

    public:
        SharedJSMessageEmitter(const SharedJSMessageEmitter&) = delete;
        SharedJSMessageEmitter& operator=(const SharedJSMessageEmitter&) = delete; 

        static SharedJSMessageEmitter& InstanceRef() 
        {
            static SharedJSMessageEmitter InstanceRef;
            return InstanceRef;
        }

        int OnMessage(const std::string& event, EventHandler handler) 
        {
            int listenerId = nextListenerId++;
            events[event].push_back(std::make_pair(listenerId, handler));
            return listenerId;
        }

        void RemoveListener(const std::string& event, int listenerId) 
        {
            auto it = events.find(event);
            if (it != events.end()) 
            {
                auto& handlers = it->second;
                handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [listenerId](const auto& handler) { return handler.first == listenerId; }), handlers.end());
            }
        }

        void EmitMessage(const std::string& event, const nlohmann::json& data) 
        {
            auto it = events.find(event);
            if (it != events.end()) 
            {
                const auto& handlers = it->second;
                for (const auto& handler : handlers) 
                {
                    handler.second(data, handler.first);
                }
            }
        }
    };

	struct JsFunctionConstructTypes
	{
		std::string pluginName, type;
	};

	const std::string ConstructFunctionCall(const char* value, const char* methodName, std::vector<JavaScript::JsFunctionConstructTypes> params);

	PyObject* EvaluateFromSocket(std::string script);
}