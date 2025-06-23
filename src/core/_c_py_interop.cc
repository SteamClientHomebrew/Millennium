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

#include "ffi.h"
#include "co_spawn.h"
#include <iostream>
#include <tuple>
#include "plugin_logger.h"
#include "fvisible.h"

using json = nlohmann::json;

/**
 * @brief Get the exception information from the Python interpreter.
 * 
 * @note This function will only handle python exceptions that have not been cleared.
 * i.e Using PyErr_Clear() or most of the Py_Simple API will automatically clear the exception.
 * 
 * @return { errorMessageString, tracebackString }
 */
MILLENNIUM std::tuple<std::string, std::string> Python::ActiveExceptionInformation() 
{
    PyObject* typeObj = nullptr;
    PyObject* valueObj = nullptr;
    PyObject* traceBackObj = nullptr;
    
    PyErr_Fetch(&typeObj, &valueObj, &traceBackObj);
    PyErr_NormalizeException(&typeObj, &valueObj, &traceBackObj);

    if (!valueObj) 
    {
        return { "Unknown Error.", "Unknown Error." };
    }

    PyObject* pStrErrorMessage = PyObject_Str(valueObj);
    const char* errorMessage = pStrErrorMessage ? PyUnicode_AsUTF8(pStrErrorMessage) : "Unknown Error.";
    
    std::string tracebackText;
    if (traceBackObj) 
    {
        PyObject* tracebackModule = PyImport_ImportModule("traceback");
        if (tracebackModule) 
        {
            PyObject* formatExceptionFunc = PyObject_GetAttrString(tracebackModule, "format_exception");
            if (formatExceptionFunc && PyCallable_Check(formatExceptionFunc)) 
            {
                PyObject* tracebackList = PyObject_CallFunctionObjArgs(formatExceptionFunc, typeObj, valueObj, traceBackObj, NULL);
                if (tracebackList) 
                {
                    PyObject* tracebackStr = PyUnicode_Join(PyUnicode_FromString(""), tracebackList);
                    if (tracebackStr) 
                    {
                        tracebackText = PyUnicode_AsUTF8(tracebackStr);
                        Py_DECREF(tracebackStr);
                    }
                    Py_DECREF(tracebackList);
                }
                Py_DECREF(formatExceptionFunc);
            }
            Py_DECREF(tracebackModule);
        }
    }

    Py_XDECREF(typeObj);
    Py_XDECREF(valueObj);
    Py_XDECREF(traceBackObj);
    Py_XDECREF(pStrErrorMessage);

    return { errorMessage, tracebackText };
}


/**
* Converts a Python object to an EvalResult with appropriate type classification and string representation.
* 
* @param {PyObject*} obj - Python object to convert, can be nullptr for error handling
* 
* @returns {Python::EvalResult} Result object containing the converted value and its classified type
* 
* @throws Does not throw C++ exceptions, but handles Python exceptions internally
*/
Python::EvalResult PyObjectCastEvalResult(PyObject* obj) 
{
    Python::EvalResult result;
    
    if (!obj) 
    {
        if (PyErr_Occurred()) 
        {
            PyObject* exc_type, *exceptionValue, *exc_traceback;
            PyErr_Fetch(&exc_type, &exceptionValue, &exc_traceback);
            if (exceptionValue) 
            {
                PyObject* exceptionString = PyObject_Str(exceptionValue);
                if (exceptionString) 
                {
                    const char* error_msg = PyUnicode_AsUTF8(exceptionString);
                    result.plain = error_msg ? error_msg : "Unknown Python error";
                    Py_DECREF(exceptionString);
                } 
                else 
                {
                    result.plain = "Failed to get error message";
                }
            } 
            else 
            {
                result.plain = "Unknown Python error occurred";
            }
            
            Py_XDECREF(exc_type);
            Py_XDECREF(exceptionValue);
            Py_XDECREF(exc_traceback);
        } 
        else 
        {
            result.plain = "Function call returned NULL";
        }

        result.type = Python::Types::Error;
        return result;
    }
    
    if (PyBool_Check(obj)) 
    {
        result.type = Python::Types::Boolean;
        result.plain = (obj == Py_True) ? "true" : "false";
    }
    else if (PyLong_Check(obj)) 
    {
        result.type = Python::Types::Integer;
        long longValue = PyLong_AsLong(obj);
        if (PyErr_Occurred()) 
        {
            PyErr_Clear();
            result.type =  Python::Types::Error;
            result.plain = "Integer overflow or conversion error";
        } 
        else 
        {
            result.plain = std::to_string(longValue);
        }
    }
    else if (PyFloat_Check(obj)) 
    {
        result.type = Python::Types::String;
        double doubleValue = PyFloat_AsDouble(obj);
        result.plain = std::to_string(doubleValue);
    }
    else if (PyUnicode_Check(obj)) 
    {
        result.type = Python::Types::String;
        const char* valueStr = PyUnicode_AsUTF8(obj);
        result.plain = valueStr ? valueStr : "";
    }
    else if (PyBytes_Check(obj)) 
    {
        result.type = Python::Types::String;
        char* bytesValue = PyBytes_AsString(obj);
        Py_ssize_t size = PyBytes_Size(obj);
        result.plain = std::string(bytesValue, size);
    }
    else if (PyList_Check(obj) || PyTuple_Check(obj) || PyDict_Check(obj)) 
    {
        result.type = Python::Types::JSON;
        
        PyObject* jsonModule = PyImport_ImportModule("json");
        if (jsonModule) 
        {
            PyObject* dumpsFunction = PyObject_GetAttrString(jsonModule, "dumps");
            if (dumpsFunction) 
            {
                PyObject* json_str = PyObject_CallFunction(dumpsFunction, "O", obj);
                if (json_str && PyUnicode_Check(json_str)) 
                {
                    const char* utf8JsonStr = PyUnicode_AsUTF8(json_str);
                    result.plain = utf8JsonStr ? utf8JsonStr : "{}";
                } 
                else 
                {
                    PyErr_Clear();
                    PyObject* representationObj = PyObject_Repr(obj);
                    if (representationObj) 
                    {
                        const char* representationString = PyUnicode_AsUTF8(representationObj);
                        result.plain = representationString ? representationString : "{}";
                        Py_DECREF(representationObj);
                    }
                    else 
                    {
                        result.plain = "{}";
                    }
                }
                Py_XDECREF(json_str);
                Py_DECREF(dumpsFunction);
            }
            Py_DECREF(jsonModule);
        } 
        else 
        {
            PyErr_Clear();
            PyObject* representationObj = PyObject_Repr(obj);
            if (representationObj) 
            {
                const char* representationStr = PyUnicode_AsUTF8(representationObj);
                result.plain = representationStr ? representationStr : "{}";
                Py_DECREF(representationObj);
            } 
            else 
            {
                result.plain = "{}";
            }
        }
    }
    else if (obj == Py_None)
    {
        result.type = Python::Types::JSON;
        result.plain = "null";
    }
    else 
    {
        result.type = Python::Types::Unknown;
        PyObject* representationObj = PyObject_Repr(obj);
        if (representationObj) 
        {
            const char* representationStr = PyUnicode_AsUTF8(representationObj);
            result.plain = representationStr ? representationStr : "<unknown object>";
            Py_DECREF(representationObj);
        } 
        else 
        {
            result.plain = "<unknown object>";
        }
    }
    
    return result;
}

/** 
 *  Resolves a dotted attribute path to a Python object.
 * 
 * @param dottedPath The dotted path string, e.g., "module.submodule.function".
 * @return A PyObject* representing the resolved attribute, or nullptr if not found.
 */
PyObject* resolveDottedAttribute(const std::string& dottedPath) 
{
    std::vector<std::string> parts;
    std::stringstream ss(dottedPath);
    std::string part;
    
    while (std::getline(ss, part, '.')) 
    {
        parts.push_back(part);
    }
    
    if (parts.empty()) 
    {
        return nullptr;
    }
    
    PyObject* current = PyObject_GetAttrString(PyImport_AddModule("__main__"), parts[0].c_str());
    if (!current) 
    {
        return nullptr;
    }
    
    for (size_t i = 1; i < parts.size(); ++i) 
    {
        PyObject* next = PyObject_GetAttrString(current, parts[i].c_str());
        Py_DECREF(current);
        if (!next) 
        {
            return nullptr;
        }
        current = next;
    }
    
    return current;
}

/** 
 * Calls a Python function with the provided JSON data.
 * 
 * @param jsonData The JSON object containing the method name and arguments.
 * @return A PyObject* representing the result of the function call, or nullptr on failure.
 */
PyObject* callPythonFunctionWithJson(const nlohmann::json& jsonData) 
{
    std::string methodName = jsonData["methodName"];
    
    PyObject* pFunc = resolveDottedAttribute(methodName);
    if (!pFunc || !PyCallable_Check(pFunc)) 
    {
        std::cerr << "Function " << methodName << " not found or not callable" << std::endl;
        Py_XDECREF(pFunc);
        return nullptr;
    }
    
    PyObject* pKwargs = PyDict_New();
    if (!pKwargs) 
    {
        Py_DECREF(pFunc);
        return nullptr;
    }
    
    if (jsonData.contains("argumentList") && !jsonData["argumentList"].is_null()) 
    {
        auto argumentList = jsonData["argumentList"];
        
        for (auto& [key, value] : argumentList.items()) 
        {
            PyObject* pValue = nullptr;
            
            if      (value.is_string())         pValue = PyUnicode_FromString(value.get<std::string>().c_str());
            else if (value.is_number_integer()) pValue = PyLong_FromLong(value.get<int>());
            else if (value.is_number_float())   pValue = PyFloat_FromDouble(value.get<double>());
            else if (value.is_boolean())        pValue = PyBool_FromLong(value.get<bool>() ? 1 : 0);
            

            else if (value.is_null()) 
            {
                pValue = Py_None;
                Py_INCREF(Py_None);
            }

            if (pValue) 
            {
                if (PyDict_SetItemString(pKwargs, key.c_str(), pValue) < 0) 
                {
                    Py_DECREF(pValue);
                    Py_DECREF(pKwargs);
                    Py_DECREF(pFunc);
                    return nullptr;
                }
                Py_DECREF(pValue);
            }
        }
    }

    PyObject* pArgs = PyTuple_New(0);
    PyObject* pResult = PyObject_Call(pFunc, pArgs, pKwargs);
    
    // Cleanup
    Py_DECREF(pArgs);
    Py_DECREF(pKwargs);
    Py_DECREF(pFunc);
    
    return pResult;
}

/**
 * Locks the Python Global Interpreter Lock (GIL) for the specified plugin's thread state
 * and evaluates the given Python script.
 *
 * @param {std::string} pluginName - The name of the plugin requesting script evaluation.
 * @param {std::string} script - The Python script to execute.
 * @returns {Python::EvalResult} - The result of the evaluation, containing the evaluated value as a string and its type.
 *
 * This function retrieves the Python thread state associated with the given plugin and locks the GIL.
 * If the thread state cannot be obtained, an error is logged, and an error result is returned.
 * The script is then executed via `EvaluatePython()`, and after execution, the GIL is released.
 *
 * Possible error conditions:
 * - If the thread state cannot be retrieved, an error is logged and returned.
 * - If the thread state is null after acquiring the GIL, an error is logged and returned.
 *
 * The function ensures that the GIL is properly acquired and released before and after execution.
 */
MILLENNIUM Python::EvalResult Python::LockGILAndInvokeMethod(std::string pluginName, nlohmann::json functionCall)
{
    const bool hasBackend = PythonManager::GetInstance().HasBackend(pluginName);

    /** Plugin has no backend and therefor the call should not be completed */
    if (!hasBackend) 
    {
        return { "false", Boolean };
    }

    auto result = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (!result.has_value()) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early?", pluginName));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", functionCall.dump()));

        return { "overstepped partying thread state", Error };
    }

    auto& [strPluginName, threadState, interpMutex] = *result.value();

    if (threadState == nullptr) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early?", pluginName));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", functionCall.dump()));

        return { "overstepped partying thread state", Error };
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);

    if (threadState == NULL) 
    {
        LOG_ERROR("script execution was queried but the receiving parties thread state was nullptr");
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", functionCall.dump()));

        return { "thread state was nullptr", Error };
    }

    PyObject* mainModule = PyImport_AddModule("__main__");

    if (!mainModule) 
    {
        const auto message = fmt::format("Failed to fetch python module on [{}]. This usually means the GIL could not be acquired either because the backend froze or crashed", pluginName);

        ErrorToLogger(pluginName, message);
        return { message, Python::Types::Error };
    }

    PyObject* globalDictionaryObj = PyModule_GetDict(mainModule);
    Python::EvalResult response = PyObjectCastEvalResult(callPythonFunctionWithJson(functionCall));

    pythonGilLock->ReleaseAndUnLockGIL();
    return response;
}

/**
 * Locks the Python Global Interpreter Lock (GIL) for the specified plugin's thread state
 * and evaluates the given Python script, discarding the result.
 *
 * @param {std::string} pluginName - The name of the plugin requesting script evaluation.
 * @param {std::string} script - The Python script to execute.
 * 
 * This function retrieves the Python thread state associated with the given plugin and locks the GIL.
 * If the thread state cannot be obtained, an error is logged, and execution is aborted.
 * 
 * The script is then executed within the `__main__` module context, but the result is discarded.
 * If an error occurs during execution, it is logged, and the function exits without returning a value.
 * 
 * Possible error conditions:
 * - If the thread state cannot be retrieved, an error is logged.
 * - If the evaluated script causes an exception, the error message and traceback are logged.
 * - If the script references an undefined function (e.g., `plugin` is not defined), a specific error is logged.
 * 
 * The function ensures that the GIL is properly acquired and released before and after execution.
 */
MILLENNIUM void Python::CallFrontEndLoaded(std::string pluginName)
{
    const bool hasBackend = PythonManager::GetInstance().HasBackend(pluginName);

    /** Plugin has no backend and therefor the call should not be completed */
    if (!hasBackend) 
    {
        return;
    }

    auto result = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (!result.has_value()) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? Tried to delegate frontend loaded message.", pluginName));
        ErrorToLogger(pluginName, fmt::format("Failed to delegate frontend loaded message for {}.", pluginName));

        return;
    }

    auto& [strPluginName, threadState, interpMutex] = *result.value();

    if (threadState == nullptr) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? Tried to delegate frontend loaded message.", pluginName));
        ErrorToLogger(pluginName, fmt::format("Failed to delegate frontend loaded message for {}.", pluginName));
        return;
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);
    {
        PyObject* globalDictionaryObj = PyModule_GetDict(PyImport_AddModule("__main__"));
        PyObject* plugin = PyDict_GetItemString(globalDictionaryObj, "plugin");

        if (plugin == nullptr) 
        {
            if (PyErr_Occurred()) 
            {
                auto [errorMsg, traceback] = Python::ActiveExceptionInformation();
                LOG_ERROR(fmt::format("Failed to get plugin attribute: {}:\n{}", errorMsg, traceback));
                ErrorToLogger(pluginName, fmt::format("Failed to get plugin attribute: {}:\n{}", errorMsg, traceback));
            }
            return;
        }

        PyObject* result = PyObject_CallMethod(plugin, "_front_end_loaded", nullptr);
        if (result == nullptr) {
            auto [errorMsg, traceback] = Python::ActiveExceptionInformation();
            LOG_ERROR(fmt::format("Failed to call _front_end_loaded: {}:\n{}", errorMsg, traceback));
            ErrorToLogger(pluginName, fmt::format("Failed to call _front_end_loaded: {}:\n{}", errorMsg, traceback));
            Py_DECREF(plugin);
            return;
        }

        Py_DECREF(result);
        // Py_DECREF(plugin); Oops this is a borrowed ref. 
        return;
    }
    pythonGilLock->ReleaseAndUnLockGIL();
}
