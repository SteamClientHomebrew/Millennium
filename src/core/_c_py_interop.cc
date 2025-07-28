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


using json = nlohmann::json;

/**
 * @brief Get the exception information from the Python interpreter.
 * 
 * @note This function will only handle python exceptions that have not been cleared.
 * i.e Using PyErr_Clear() or most of the Py_Simple API will automatically clear the exception.
 * 
 * @return { errorMessageString, tracebackString }
 */
std::tuple<std::string, std::string> Python::ActiveExceptionInformation() 
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

namespace Python
{
    namespace ObjectHandler
    {
        EvalResult HandleActiveException() 
        {
            const auto [errorMessage, tracebackText] = ActiveExceptionInformation();
            return {Types::Error, errorMessage + tracebackText};
        }

        EvalResult HandleBooleanObject(PyObject* obj) 
        {
            return {Types::Boolean, (obj == Py_True) ? "True" : "False"};
        }

        EvalResult HandleIntegerObject(PyObject* obj) 
        {
            long longValue = PyLong_AsLong(obj);
            if (PyErr_Occurred()) return HandleActiveException();
            return {Types::Integer, std::to_string(longValue)};
        }

        EvalResult HandleFloatObject(PyObject* obj) 
        {
            double doubleValue = PyFloat_AsDouble(obj);
            if (PyErr_Occurred()) return HandleActiveException();
            return {Types::String, std::to_string(doubleValue)};
        }

        EvalResult HandleUnicodeObject(PyObject* obj)
        {
            const char* valueStr = PyUnicode_AsUTF8(obj);
            if (PyErr_Occurred()) return HandleActiveException();
            return {Types::String, valueStr ? std::string(valueStr) : std::string("")};
        }

        EvalResult HandleBytesObject(PyObject* obj) 
        {
            char* bytesValue = PyBytes_AsString(obj);
            if (!bytesValue) return {Types::Error, "Python::ObjectHandler::HandleBytesObject(): Failed to convert bytes object to string."};

            Py_ssize_t size = PyBytes_Size(obj);
            if (size < 0) return {Types::Error, "Python::ObjectHandler::HandleBytesObject(): Failed to get size of bytes object."};

            return {Types::String, std::string(bytesValue, size)};
        }

        EvalResult HandleContainerObject(PyObject* obj) 
        {
            PyObject* jsonModule = PyImport_ImportModule("json");
            if (jsonModule) 
            {
                PyObject* dumpsFunction = PyObject_GetAttrString(jsonModule, "dumps");
                if (dumpsFunction) 
                {
                    PyObject* jsonStr = PyObject_CallFunction(dumpsFunction, "O", obj);
                    if (jsonStr && PyUnicode_Check(jsonStr)) 
                    {
                        const char* utf8JsonStr = PyUnicode_AsUTF8(jsonStr);
                        std::string result = utf8JsonStr ? utf8JsonStr : "{}";
                        Py_XDECREF(jsonStr);
                        Py_DECREF(dumpsFunction);
                        Py_DECREF(jsonModule);
                        return {Types::JSON, result};
                    }
                    Py_XDECREF(jsonStr);
                    Py_DECREF(dumpsFunction);
                }
                Py_DECREF(jsonModule);
            }
            
            PyErr_Clear();
            PyObject* objRepresentation = PyObject_Repr(obj);
            if (objRepresentation) 
            {
                const char* unicodeRepresentation = PyUnicode_AsUTF8(objRepresentation);
                std::string result = unicodeRepresentation ? unicodeRepresentation : "{}";
                Py_DECREF(objRepresentation);
                return {Types::JSON, result};
            }
            
            return {Types::JSON, "{}"};
        }

        EvalResult HandleNoneObject() 
        {
            return {Types::JSON, "null"};
        }

        EvalResult HandleUnknownObject(PyObject* obj) 
        {
            PyObject* objRepresentation = PyObject_Repr(obj);
            if (objRepresentation) 
            {
                const char* unicodeRepresentation = PyUnicode_AsUTF8(objRepresentation);
                std::string result = unicodeRepresentation ? unicodeRepresentation : "<unknown object>";
                Py_DECREF(objRepresentation);
                return {Types::Unknown, result};
            }
            return {Types::Unknown, "<unknown object>"};
        }
    }
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
    if (!obj) return Python::ObjectHandler::HandleActiveException();
    
    if      (PyBool_Check(obj))                                            return Python::ObjectHandler::HandleBooleanObject(obj);
    else if (PyLong_Check(obj))                                            return Python::ObjectHandler::HandleIntegerObject(obj);
    else if (PyFloat_Check(obj))                                           return Python::ObjectHandler::HandleFloatObject(obj);
    else if (PyUnicode_Check(obj))                                         return Python::ObjectHandler::HandleUnicodeObject(obj);
    else if (PyBytes_Check(obj))                                           return Python::ObjectHandler::HandleBytesObject(obj);
    else if (PyList_Check(obj) || PyTuple_Check(obj) || PyDict_Check(obj)) return Python::ObjectHandler::HandleContainerObject(obj);
    else if (obj == Py_None)                                               return Python::ObjectHandler::HandleNoneObject();

    else return Python::ObjectHandler::HandleUnknownObject(obj);
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

/*
 * Returns a representation of the given JSON data as a PyObject*
 *
 * @param json The JSON object to be converted
 * @return A PyObject* representing the passed JSON object
 */
PyObject* PyObjectFromJson(const nlohmann::json& json)
{
    PyObject* pObject = nullptr;
    
    if      (json.is_string())         pObject = PyUnicode_FromString(json.get<std::string>().c_str());
    else if (json.is_number_integer()) pObject = PyLong_FromLongLong(json.get<long long>());
    else if (json.is_number_float())   pObject = PyFloat_FromDouble(json.get<double>());
    else if (json.is_boolean())        pObject = PyBool_FromLong(json.get<bool>() ? 1 : 0);
    else if (json.is_array())
    {
        pObject = PyList_New(json.size());

        if (!pObject) 
        {
            LOG_ERROR("Failed to allocate a new Python list of size: {}", json.size());
            return nullptr;
        }

        int i = 0;
        for (const auto& arrayItem : json)
        {
            PyObject* pListItem = PyObjectFromJson(arrayItem);
            if (pListItem)
            {
                if (PyList_SetItem(pObject, i, pListItem) < 0)
                {
                    Py_DECREF(pObject);
                    LOG_ERROR("Failed to set item in Python list at index: {}, json data: {}", i, json.dump(4));
                    return nullptr;
                }
                i++;
            }
            else
            {
                // Clean up and return error if recursive call failed
                Py_DECREF(pObject);
                LOG_ERROR("Failed to convert array item to Python object");
                return nullptr;
            }
        }
    }
    else if (json.is_null()) 
    {
        pObject = Py_None;
        Py_INCREF(Py_None);
    }

    return pObject;
}

/** 
 * Calls a Python function with the provided JSON data.
 * 
 * @param jsonData The JSON object containing the method name and arguments.
 * @return A PyObject* representing the result of the function call, or nullptr on failure.
 */
PyObject* InvokePythonFunction(const nlohmann::json& jsonData) 
{
    std::string methodName = jsonData["methodName"];
    
    PyObject* pFunc = resolveDottedAttribute(methodName);
    if (!pFunc || !PyCallable_Check(pFunc)) 
    {
        LOG_ERROR(fmt::format("Failed to resolve or call function: {}. Call data {}", methodName, jsonData.dump(4)));
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
            PyObject* pValue = PyObjectFromJson(value);

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
Python::EvalResult Python::LockGILAndInvokeMethod(std::string pluginName, nlohmann::json functionCall)
{
    const bool hasBackend = PythonManager::GetInstance().HasBackend(pluginName);

    /** Plugin has no backend and therefor the call should not be completed */
    if (!hasBackend) 
    {
        return { Boolean, "false" };
    }

    auto result = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (!result.has_value()) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early?", pluginName));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", functionCall.dump()));

        return { Error, "overstepped partying thread state" };
    }

    auto& [strPluginName, threadState, interpMutex] = *result.value();

    if (threadState == nullptr) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early?", pluginName));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", functionCall.dump()));

        return { Error, "overstepped partying thread state" };
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);

    if (threadState == NULL) 
    {
        LOG_ERROR("script execution was queried but the receiving parties thread state was nullptr");
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", functionCall.dump()));

        return { Error, "thread state was nullptr" };
    }

    PyObject* mainModule = PyImport_AddModule("__main__");

    if (!mainModule) 
    {
        const auto message = fmt::format("Failed to fetch python module on [{}]. This usually means the GIL could not be acquired either because the backend froze or crashed", pluginName);

        ErrorToLogger(pluginName, message);
        return { Error, message };
    }

    PyObject* globalDictionaryObj = PyModule_GetDict(mainModule);
    Python::EvalResult response = PyObjectCastEvalResult(InvokePythonFunction(functionCall));

    if (response.type == Python::Types::Error) 
    {
        LOG_ERROR(fmt::format("Failed to evaluate script: {}. Error: {}", functionCall.dump(), response.plain));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}. Error: {}", functionCall.dump(), response.plain));
    }

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
void Python::CallFrontEndLoaded(std::string pluginName)
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
