#include "ffi.h"
#include <core/py_controller/co_spawn.h>
#include <core/loader.h>
#include <future>

struct EvalResult {
    nlohmann::basic_json<> json;
    bool successfulCall;
};

#define SHARED_JS_EVALUATE_ID 54999

#include <mutex>
#include <condition_variable>

const EvalResult ExecuteOnSharedJsContext(std::string javaScriptEval) 
{
    std::mutex mtx;
    std::condition_variable cv;
    EvalResult evalResult;
    bool resultReady = false;  // Flag to indicate when the result is ready

    bool messageSendSuccess = Sockets::PostShared(nlohmann::json({ 
        { "id", SHARED_JS_EVALUATE_ID },
        { "method", "Runtime.evaluate" }, 
        { "params", {
            { "expression", javaScriptEval }, 
            { "awaitPromise", true }
        }} 
    }));

    if (!messageSendSuccess) 
    {
        throw std::runtime_error("couldn't send message to socket");
    }

    int listenerId = JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", [&](const nlohmann::json& eventMessage, int listenerId) 
    {
        std::lock_guard<std::mutex> lock(mtx);  // Lock mutex for safe access

        nlohmann::json response = eventMessage;
        std::string method = response.value("method", std::string());

        if (method.find("Debugger") != std::string::npos || method.find("Console") != std::string::npos) 
        {
            return;
        }

        try 
        {
            if (!response.contains("id") || (response.contains("id") && !response["id"].is_null() && response["id"] != SHARED_JS_EVALUATE_ID))
            {
                return;
            }

            if (response["result"].contains("exceptionDetails"))
            {
                const std::string classType = response["result"]["exceptionDetails"]["exception"]["className"];

                // Custom exception type thrown from CallFrontendMethod in executor.cc
                if (classType == "MillenniumFrontEndError") 
                    evalResult = { "__CONNECTION_ERROR__", false };
                else
                    evalResult = { response["result"]["exceptionDetails"]["exception"]["description"], false };
            }
            else 
            {
                evalResult = { response["result"]["result"], true };
            }

            resultReady = true;
            cv.notify_one();  // Signal that result is ready
            JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
        }
        catch (nlohmann::detail::exception& ex) 
        {
            LOG_ERROR(fmt::format("JavaScript::SharedJSMessageEmitter error -> {}", ex.what()));
        }
    });

    // Wait for the result to be ready
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return resultReady; });

    if (!evalResult.successfulCall && evalResult.json == "__CONNECTION_ERROR__") 
    {
        throw std::runtime_error("frontend is not loaded!");
    }

    return evalResult;
}

const std::string JavaScript::ConstructFunctionCall(const char* plugin, const char* methodName, std::vector<JavaScript::JsFunctionConstructTypes> fnParams)
{
    std::string strFunctionFormatted = fmt::format("PLUGIN_LIST['{}'].{}(", plugin, methodName);

    std::map<std::string, std::string> boolMap
    {
        { "True", "true" },
        { "False", "false" }
    };

    for (auto iterator = fnParams.begin(); iterator != fnParams.end(); ++iterator) 
    {
        auto& param = *iterator;

        switch (param.type)
        {
            case JavaScript::Types::String: 
            {
                strFunctionFormatted += fmt::format("\"{}\"", param.pluginName);
                break;
            }
            case JavaScript::Types::Boolean: 
            {
                strFunctionFormatted += boolMap[param.pluginName];
                break;
            }
            default: 
            {
                strFunctionFormatted += param.pluginName;
                break;
            }
        }

        if (std::next(iterator) != fnParams.end()) 
        {
            strFunctionFormatted += ", ";
        }
    }
    strFunctionFormatted += ");"; return strFunctionFormatted;
}

PyObject* JavaScript::EvaluateFromSocket(std::string script)
{
    try 
    {
        EvalResult response = ExecuteOnSharedJsContext(script);

        if (!response.successfulCall) 
        {
            PyErr_SetString(PyExc_RuntimeError, response.json.get<std::string>().c_str());
            return NULL;
        }

        std::string type = response.json["type"];

        if      (type == "string") return PyUnicode_FromString(response.json["value"].get<std::string>().c_str());
        else if (type == "bool")   return PyBool_FromLong(response.json["value"]);
        else if (type == "int")    return PyLong_FromLong(response.json["value"]);
        else
            return PyUnicode_FromString("Js function returned unaccepted type. accepted types [string, bool, int]");

    }
    catch (nlohmann::detail::exception& ex)
    {
        std::string message = fmt::format("Millennium couldn't decode the response from {}, reason: {}", script, ex.what());
        return PyUnicode_FromString(message.c_str());
    }
    catch (std::exception&)
    {
        PyErr_SetString(PyExc_ConnectionError, "frontend is not loaded!");
        return NULL;
    }

    Py_RETURN_NONE;
}
