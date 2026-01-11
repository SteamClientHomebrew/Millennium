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

#include "millennium/ffi.h"
#include "millennium/init.h"
#include "millennium/types.h"
#include <condition_variable>
#include <mutex>

/**
 * Executes JavaScript code on the SharedJSContext and retrieves the result.
 *
 * @param {std::string} javaScriptEval - The JavaScript expression to evaluate.
 * @returns {EvalResult} - The result of the evaluation, containing the evaluated value and a success flag.
 *
 * This function sends a message via a socket to request JavaScript evaluation. It listens for a response,
 * processes the result, and stores it in `evalResult`. The function waits for the result using a
 * condition variable before returning.
 *
 * Error handling:
 * - If the message cannot be sent, an exception is thrown.
 * - If an exception occurs in the JavaScript execution, the error description is returned in `evalResult`.
 * - If the frontend is not loaded, an exception is thrown.
 *
 * Synchronization:
 * - Uses a `std::mutex` and `std::condition_variable` to ensure safe access to shared data.
 * - The listener is removed once a response is received.
 */
JsEvalResult JavaScript::ExecuteOnSharedJsContext(std::string javaScriptEval)
{
    JsEvalResult ret;

    const json params = {
        { "expression",   javaScriptEval },
        { "awaitPromise", true           }
    };

    auto result = m_cdp->send("Runtime.evaluate", params).get();

    if (result.contains("exceptionDetails")) {
        const std::string classType = result["exceptionDetails"]["exception"]["className"];

        if (classType == "MillenniumFrontEndError")
            ret = { "__CONNECTION_ERROR__", false };
        else
            ret = { result["exceptionDetails"]["exception"]["description"], false };
    } else {
        ret = { result["result"], true };
    }

    return ret;
}

/**
 * Escapes special characters in a JavaScript string.
 */
std::string EscapeJavaScriptString(const std::string& input)
{
    std::ostringstream escaped;

    for (size_t i = 0; i < input.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        switch (c) {
            case '\"':
                escaped << "\\\"";
                break;
            case '\\':
                escaped << "\\\\";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (c < 0x20) {
                    // Escape control characters as \uXXXX
                    escaped << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else if (c == 0xE2 && i + 2 < input.length()) {
                    // Check for UTF-8 encoded U+2028 (0xE2 0x80 0xA8) and U+2029 (0xE2 0x80 0xA9)
                    unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                    unsigned char c2 = static_cast<unsigned char>(input[i + 2]);

                    if (c1 == 0x80 && c2 == 0xA8) {
                        escaped << "\\u2028";
                        i += 2;
                    } else if (c1 == 0x80 && c2 == 0xA9) {
                        escaped << "\\u2029";
                        i += 2;
                    } else {
                        escaped << c;
                    }
                } else {
                    escaped << c;
                }
                break;
        }
    }

    return escaped.str();
}

/**
 * Constructs a JavaScript function call string for a given plugin and method.
 *
 * @param {const char*} plugin - The name of the plugin containing the JavaScript function.
 * @param {const char*} methodName - The name of the method to be called on the plugin.
 * @param {std::vector<JavaScript::JsFunctionConstructTypes>} fnParams - A list of function parameters,
 *        each containing a type and value.
 * @returns {std::string} - A formatted JavaScript function call string.
 *
 * This function builds a JavaScript function call in the format:
 * `PLUGIN_LIST['pluginName'].methodName(param1, param2, ...)`
 *
 * Parameter handling:
 * - Strings are enclosed in double quotes.
 * - Boolean values are converted from `"True"`/`"False"` to JavaScript `true`/`false`.
 * - Other types are inserted as-is.
 *
 * If multiple parameters exist, they are separated by commas.
 */
const std::string JavaScript::ConstructFunctionCall(const char* plugin, const char* methodName, std::vector<JavaScript::JsFunctionConstructTypes> fnParams)
{
    std::string strFunctionFormatted = fmt::format("PLUGIN_LIST['{}'].{}(", plugin, methodName);

    std::map<std::string, std::string> boolMap{
        /** compatibility with Python */
        { "True",  "true"  },
        { "False", "false" },
        /** compatibility with lua */
        { "true",  "true"  },
        { "false", "false" }
    };

    for (auto iterator = fnParams.begin(); iterator != fnParams.end(); ++iterator) {
        auto& param = *iterator;

        switch (param.type) {
            case JavaScript::Types::String:
                strFunctionFormatted += fmt::format("\"{}\"", EscapeJavaScriptString(param.pluginName));
                break;
            case JavaScript::Types::Boolean:
                strFunctionFormatted += boolMap[param.pluginName];
                break;
            default:
                strFunctionFormatted += param.pluginName;
                break;
        }

        if (std::next(iterator) != fnParams.end()) {
            strFunctionFormatted += ", ";
        }
    }
    strFunctionFormatted += ");";
    return strFunctionFormatted;
}

/**
 * Evaluates a JavaScript script via a shared socket connection and converts the result to a PyObject.
 *
 * @param {std::string} script - The JavaScript code to be executed.
 * @returns {PyObject*} - A Python object representing the evaluation result.
 *
 * This function sends the script for evaluation using `ExecuteOnSharedJsContext()`, processes the response,
 * and converts the returned JavaScript value into an appropriate Python type:
 * - JavaScript strings → `PyUnicode_FromString`
 * - JavaScript booleans → `PyBool_FromLong`
 * - JavaScript numbers → `PyLong_FromLong`
 * - Unsupported types result in a formatted string indicating an error.
 *
 * Error Handling:
 * - If the execution fails, a Python `RuntimeError` is raised with the provided error message.
 * - If the frontend is not loaded, a Python `ConnectionError` is set.
 * - If the response cannot be parsed, an error message is returned as a Python string.
 */
PyObject* JavaScript::Py_EvaluateFromSocket(std::string script)
{
    try {
        JsEvalResult response = ExecuteOnSharedJsContext(script);

        if (!response.successfulCall) {
            PyErr_SetString(PyExc_RuntimeError, response.json.get<std::string>().c_str());
            return NULL;
        }

        std::string type = response.json["type"];

        if (type == "string")
            return PyUnicode_FromString(response.json["value"].get<std::string>().c_str());
        else if (type == "boolean")
            return PyBool_FromLong(response.json["value"]);
        else if (type == "number")
            return PyLong_FromLong(response.json["value"]);
        else
            return PyUnicode_FromString(fmt::format("Js function returned unaccepted type '{}'. Accepted types [string, boolean, number]", type).c_str());
    } catch (nlohmann::detail::exception& ex) {
        std::string message = fmt::format("Millennium couldn't decode the response from {}, reason: {}", script, ex.what());
        return PyUnicode_FromString(message.c_str());
    } catch (std::exception&) {
        PyErr_SetString(PyExc_ConnectionError, "frontend is not loaded!");
        return NULL;
    }

    Py_RETURN_NONE;
}

int JavaScript::Lua_EvaluateFromSocket(std::string script, lua_State* L)
{
    try {
        JsEvalResult response = ExecuteOnSharedJsContext(script);

        if (!response.successfulCall) {
            lua_pushnil(L);
            lua_pushstring(L, response.json.get<std::string>().c_str());
            return 2; // (nil, error)
        }

        std::string type = response.json["type"];

        if (type == "string") {
            lua_pushstring(L, response.json["value"].get<std::string>().c_str());
            return 1;
        } else if (type == "boolean") {
            lua_pushboolean(L, response.json["value"]);
            return 1;
        } else if (type == "number") {
            lua_pushinteger(L, response.json["value"]);
            return 1;
        } else {
            lua_pushnil(L);
            std::string msg = fmt::format("Js function returned unaccepted type '{}'. Accepted types [string, boolean, number]", type);
            lua_pushstring(L, msg.c_str());
            return 2;
        }
    } catch (const nlohmann::detail::exception& ex) {
        std::string message = fmt::format("Millennium couldn't decode the response from {}, reason: {}", script, ex.what());
        lua_pushnil(L);
        lua_pushstring(L, message.c_str());
        return 2;
    } catch (const std::exception&) {
        lua_pushnil(L);
        lua_pushstring(L, "frontend is not loaded!");
        return 2;
    }
}
