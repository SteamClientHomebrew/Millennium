#include "mutex_impl.hpp"
#include <core/backend/co_spawn.hpp>
#include <core/loader.hpp>
#include <future>

struct shared_response {
    nlohmann::basic_json<> json;
    bool _success;
};

const shared_response execute_shared(std::string javascript) {
    // Create a promise to store the result
    std::promise<shared_response> promise;

    bool success = tunnel::post_shared(nlohmann::json({ 
        {"id", 54999}, 
        {"method", "Runtime.evaluate"}, 
        {"params", {
            {"expression", javascript}, 
            {"awaitPromise", true}
        }} 
    }));

    if (!success) {
        throw std::runtime_error("couldn't send message to socket");
    }

    // Register message handler to capture the result
    javascript::emitter::instance().on("msg", [&](const nlohmann::json& event_msg, int listenerId) {
        try {
            if (event_msg.contains("id") && event_msg["id"] == 54999) {

                if (event_msg["result"].contains("exceptionDetails")) 
                {
                    promise.set_value({ 
                        event_msg["result"]["exceptionDetails"]["exception"]["description"], false
                    });
                    return;
                }

                promise.set_value({ event_msg["result"]["result"], true });

                // Set the value of the promise with the result
                javascript::emitter::instance().off("msg", listenerId);
            }
        }
        catch (nlohmann::detail::exception& ex) {
            console.err(fmt::format("javascript::emitter err -> {}", ex.what()));
        }
        catch (std::future_error& ex) {
            console.err(ex.what());
        }
    });

    // Return the result obtained from the promise
    return promise.get_future().get();
}

const void javascript::reload_shared_context() {
    return (void)tunnel::post_shared({ {"id", 89}, {"method", "Page.reload"}, {"sessionId", sessionId} });
}

const std::string javascript::construct_fncall(
    const char* plugin, const char* method_name, std::vector<javascript::param_types> params)
{
    // plugin module exports are stored in a map on the window object PLUGIN_LIST
    std::string out = fmt::format("PLUGIN_LIST['{}'].{}(", plugin, method_name);

    for (auto it = params.begin(); it != params.end(); ++it) 
    {
        auto& param = *it;

        if (param.type == "str") { out += fmt::format("\"{}\"", param.name); }
        else if (param.type == "bool") { out += param.name == "True" ? "true" : "false"; } //python decided to be quirky with caps
        else { out += param.name; }

        if (std::next(it) != params.end()) {
            out += ", ";
        }
    }
    out += ");"; return out;
}

PyObject* javascript::evaluate_lock(std::string script)
{
    try 
    {
        auto response = execute_shared(script);

        if (!response._success) {
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
        std::string message = fmt::format("Millennium couldn't decode the response from {}", script);
        return PyUnicode_FromString(message.c_str());
    }
    catch (std::exception& ex)
    {
        PyErr_SetString(PyExc_ConnectionError, "frontend is not loaded!");
        return NULL;
    }

    Py_RETURN_NONE;
}
