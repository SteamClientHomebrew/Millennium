#include "ffi.hpp"
#include <core/py_controller/co_spawn.hpp>
#include <core/loader.hpp>
#include <future>

struct EvalResult {
    nlohmann::basic_json<> json;
    bool successfulCall;
};

#define SHARED_JS_EVALUATE_ID 54999

const EvalResult ExecuteOnSharedJsContext(std::string javaScriptEval) 
{
    std::promise<EvalResult> promise;

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

    JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", [&](const nlohmann::json& eventMessage, int listenerId) 
    {
        try 
        {
            if (eventMessage.contains("id") && eventMessage["id"] != SHARED_JS_EVALUATE_ID)
            {
                return;
            }

            if (eventMessage["result"].contains("exceptionDetails"))
            {
                promise.set_value
                ({
                    eventMessage["result"]["exceptionDetails"]["exception"]["description"], false
                });
                return;
            }

            promise.set_value({ eventMessage["result"]["result"], true });
            JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
        }
        catch (nlohmann::detail::exception& ex) 
        {
            Logger.Error(fmt::format("JavaScript::SharedJSMessageEmitter error -> {}", ex.what()));
        }
        catch (std::future_error& exception) 
        {
            Logger.Error(exception.what());
        }
    });

    return promise.get_future().get();
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

        if (param.type == "str") 
        {
            strFunctionFormatted += fmt::format("\"{}\"", param.pluginName); 
        }
        else if (param.type == "bool") 
        { 
            strFunctionFormatted += boolMap[param.pluginName];
        }
        else 
        { 
            strFunctionFormatted += param.pluginName; 
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
