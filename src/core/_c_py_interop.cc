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
#include "logger.h"
#include "fvisible.h"

using json = nlohmann::json;

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
* Converts a JSON value to its corresponding Python object representation.
* 
* @param {json&} value - Reference to a nlohmann::json object to be converted
* 
* @returns {PyObject*} Python object representing the JSON value, or nullptr on conversion failure
* 
* @note The caller is responsible for decrementing the reference count of the returned PyObject*
* @note For JSON objects, all keys are converted to Python Unicode strings regardless of the original key type
* @note On failure during array or object conversion, all partially constructed objects are properly cleaned up
* 
* @returns {nullptr} When memory allocation fails or nested conversion fails
*/
PyObject* JsonToPyObject(const json& value) 
{
    if (value.is_null()) 
    {
        Py_RETURN_NONE;
    }
    else if (value.is_boolean()) 
    {
        if (value.get<bool>()) 
        {
            Py_RETURN_TRUE;
        } 
        else {
            Py_RETURN_FALSE;
        }
    }
    else if (value.is_number_integer()) 
    {
        return PyLong_FromLong(value.get<long>());
    }
    else if (value.is_number_float()) 
    {
        return PyFloat_FromDouble(value.get<double>());
    }
    else if (value.is_string()) 
    {
        return PyUnicode_FromString(value.get<std::string>().c_str());
    }
    else if (value.is_array()) 
    {
        PyObject* list = PyList_New(value.size());
        for (size_t i = 0; i < value.size(); i++) 
        {
            PyObject* item = JsonToPyObject(value[i]);
            if (!item) 
            {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SetItem(list, i, item);
        }
        return list;
    }
    else if (value.is_object()) 
    {
        PyObject* dict = PyDict_New();
        for (auto& [key, val] : value.items()) 
        {
            PyObject* pyKey = PyUnicode_FromString(key.c_str());
            PyObject* pyValue = JsonToPyObject(val);
            if (!pyKey || !pyValue) 
            {
                Py_XDECREF(pyKey);
                Py_XDECREF(pyValue);
                Py_DECREF(dict);
                return nullptr;
            }
            PyDict_SetItem(dict, pyKey, pyValue);
            Py_DECREF(pyKey);
            Py_DECREF(pyValue);
        }
        return dict;
    }
    
    Py_RETURN_NONE;
}

/**
 * Splits a dot-separated method name into its components.
 * 
 * @param {std::string} methodName - The dot-separated method name to split (e.g., "os.path.join").
 * @returns {std::vector<std::string>} A vector containing the individual components of the method name.
 */
std::vector<std::string> SplitMethodName(const std::string& methodName) 
{
    std::string current;
    std::vector<std::string> parts;
    
    for (char c : methodName) 
    {
        if (c == '.') 
        {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } 
        else 
        {
            current += c;
        }
    }
    
    if (!current.empty()) 
    {
        parts.push_back(current);
    }
    
    return parts;
}

/**
* Resolves a callable Python object from a dot-separated method path.
* 
* @param {std::vector<std::string>&} methodComponents - Vector of string components representing the method path (e.g., ["os", "path", "join"])
* @param {PyObject*} globals_dict - Python dictionary object containing the global namespace to search in
* 
* @returns {PyObject*} Python callable object if found, nullptr if not found or on error
* 
* @description
* This function resolves callable objects using a hierarchical lookup strategy:

* @throws {PyExc_ValueError} When methodComponents vector is empty
* @throws {PyExc_NameError} When the root component cannot be found in any namespace
* @throws {PyExc_AttributeError} When a component in the path chain doesn't exist as an attribute
* 
* @note The caller is responsible for decrementing the reference count of the returned PyObject*
* 
* @example
* // Resolving a simple function
* methodComponents = ["print"]
* 
* @example
* // Resolving a nested module function
* methodComponents = ["os", "path", "join"]
* 
* @example
* // Resolving a method from an imported module
* methodComponents = ["json", "dumps"]
*/
PyObject* ResolveCallable(const std::vector<std::string>& methodComponents, PyObject* globals_dict) 
{
    if (methodComponents.empty()) 
    {
        PyErr_SetString(PyExc_ValueError, "Empty method path");
        return nullptr;
    }
    
    PyObject* current_obj = nullptr;
    current_obj = PyDict_GetItemString(globals_dict, methodComponents[0].c_str());

    if (current_obj) 
    {
        Py_INCREF(current_obj);
    } 
    else 
    {
        current_obj = PyImport_ImportModule(methodComponents[0].c_str());
        if (!current_obj) 
        {
            PyErr_Clear();
            PyObject* builtins = PyEval_GetBuiltins();
            current_obj = PyDict_GetItemString(builtins, methodComponents[0].c_str());

            if (current_obj) 
            {
                Py_INCREF(current_obj);
            } 
            else
            {
                PyErr_Format(PyExc_NameError, "Name '%s' not found", methodComponents[0].c_str());
                return nullptr;
            }
        } 
    }
    
    for (size_t i = 1; i < methodComponents.size(); i++) 
    {
        PyObject* next_obj = PyObject_GetAttrString(current_obj, methodComponents[i].c_str());
        Py_DECREF(current_obj);
        
        if (!next_obj) 
        {
            PyErr_Format(PyExc_AttributeError, "Object has no attribute '%s'", methodComponents[i].c_str());
            return nullptr;
        }
        
        current_obj = next_obj;
    }

    return current_obj;
}

/**
* Invokes a Python method synchronously from a JSON specification.
* 
* @param {Object} function_spec - JSON object containing the function call specification
* @param {string} function_spec.methodName - The name of the Python method to call (supports dot notation for nested attributes)
* @param {Array|Object|*} [function_spec.argumentList] - Optional arguments for the method call
* @param {PyObject*} globals_dict - Python dictionary object containing the global namespace to resolve the method from
* 
* @returns {Python::EvalResult} Result object containing the method's return value and type information
* 
* @description
* This function supports multiple argument formats:
* - Array: Positional arguments ["arg1", "arg2", ...]
* - Object with name/value: Single keyword argument {"name": "param_name", "value": param_value}
* - Object with key/value pairs: Multiple keyword arguments {"param1": value1, "param2": value2}
* - Single value: Single positional argument
* - No argumentList: Method called with no arguments
* 
* The methodName supports dot notation (e.g., "module.submodule.function") to access nested attributes.
* 
* @throws {Python::Types::Error} When JSON is malformed, method cannot be resolved, or Python execution fails
*/
Python::EvalResult InvokeMethodSync(const json& function_spec, PyObject* globals_dict) 
{
    try 
    {
        if (!function_spec.contains("methodName")) 
        {
            return { "JSON must contain 'methodName' field", Python::Types::Error };
        }
        
        std::string method_name = function_spec["methodName"].get<std::string>();
        std::vector<std::string> path_parts = SplitMethodName(method_name);
        
        PyObject* callable = ResolveCallable(path_parts, globals_dict);
        if (!callable) 
        {
            return PyObjectCastEvalResult(nullptr); 
        }
        
        if (!function_spec.contains("argumentList")) 
        {
            PyObject* result = PyObject_CallObject(callable, nullptr);
            Py_DECREF(callable);
            
            Python::EvalResult eval_result = PyObjectCastEvalResult(result);
            Py_XDECREF(result);
            return eval_result;
        }
        
        const json& argumentList = function_spec["argumentList"];
    
        PyObject* result = nullptr;
        
        if (argumentList.is_array()) 
        {
            PyObject* argsTuple = PyTuple_New(argumentList.size());
            
            for (size_t i = 0; i < argumentList.size(); i++) 
            {
                PyObject* arg = JsonToPyObject(argumentList[i]);
                if (!arg) 
                {
                    Py_DECREF(argsTuple);
                    Py_DECREF(callable);
                    return { "Failed to convert JSON argument to Python object", Python::Types::Error};
                }
                PyTuple_SetItem(argsTuple, i, arg);
            }
            
            result = PyObject_CallObject(callable, argsTuple);
            Py_DECREF(argsTuple);
        }
        else if (argumentList.is_object()) 
        {
            if (argumentList.contains("name") && argumentList.contains("value")) 
            {
                std::string param_name = argumentList["name"].get<std::string>();
                PyObject* param_value = JsonToPyObject(argumentList["value"]);
                
                if (!param_value) 
                {
                    Py_DECREF(callable);
                    return { "Failed to convert JSON value to Python object", Python::Types::Error};
                }
                
                PyObject* kwargs = PyDict_New();
                PyDict_SetItemString(kwargs, param_name.c_str(), param_value);
                
                result = PyObject_Call(callable, PyTuple_New(0), kwargs);
                
                Py_DECREF(param_value);
                Py_DECREF(kwargs);
            }
            else 
            {
                PyObject* kwargs = PyDict_New();
                
                for (auto& [key, value] : argumentList.items()) 
                {
                    PyObject* py_value = JsonToPyObject(value);
                    if (!py_value) 
                    {
                        Py_DECREF(kwargs);
                        Py_DECREF(callable);
                        return { "Failed to convert JSON argument to Python object", Python::Types::Error};
                    }
                    
                    PyDict_SetItemString(kwargs, key.c_str(), py_value);
                    Py_DECREF(py_value);
                }
                result = PyObject_Call(callable, PyTuple_New(0), kwargs);
                Py_DECREF(kwargs);
            }
        }
        else 
        {
            PyObject* arg = JsonToPyObject(argumentList);
            if (!arg) 
            {
                Py_DECREF(callable);
                return { "Failed to convert JSON argument to Python object", Python::Types::Error};
            }
            
            PyObject* argsTuple = PyTuple_New(1);
            PyTuple_SetItem(argsTuple, 0, arg);
            
            result = PyObject_CallObject(callable, argsTuple);
            Py_DECREF(argsTuple);
        }
        
        Py_DECREF(callable);
        Python::EvalResult evalResult = PyObjectCastEvalResult(result);
        Py_XDECREF(result);

        return evalResult;
        
    } 
    catch (const json::exception& e) 
    {
        return { std::string("JSON parsing error: ") + e.what(), Python::Types::Error};
    }
}

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
 * Converts a Python object to a JSON string using the `json.dumps()` function.
 *
 * @param {PyObject*} obj - The Python object to convert to JSON.
 * @returns {std::optional<std::string>} - The JSON string representation of the object, or an empty optional if conversion fails.
 *
 * This function attempts to serialize a Python object into a JSON string using the `json` module's `dumps()` function.
 * If the conversion is successful, it returns the JSON string. If it fails, it returns an empty optional.
 */
MILLENNIUM const std::optional<std::string> PyObjectToJsonString(PyObject* obj) 
{
    // Import json module
    static PyObject* jsonModule = PyImport_ImportModule("json");
    if (!jsonModule) 
    {
        return std::nullopt;
    }

    // Get dumps function
    static PyObject* jsonDumpsFunc = PyObject_GetAttrString(jsonModule, "dumps");
    if (!jsonDumpsFunc || !PyCallable_Check(jsonDumpsFunc)) 
    {
        return std::nullopt;
    }

    PyObject* args = PyTuple_Pack(1, obj);
    if (!args) 
    {
        return std::nullopt;
    }

    PyObject* jsonStrObj = PyObject_CallObject(jsonDumpsFunc, args);
    Py_DECREF(args);

    if (!jsonStrObj) 
    {
        PyErr_Clear();  // Clear any error from json.dumps call failure
        return std::nullopt;
    }

    const char* jsonCStr = PyUnicode_AsUTF8(jsonStrObj);
    std::string jsonStr = jsonCStr ? jsonCStr : "";

    Py_DECREF(jsonStrObj);
    return jsonStr;
}


/**
 * Evaluates a given Python script within the `__main__` module context.
 *
 * @param {std::string} pluginName - The name of the plugin invoking the evaluation.
 * @param {std::string} script - The Python script to execute.
 * @returns {Python::EvalResult} - The result of the evaluation, containing the evaluated value as a string and its type.
 *
 * The function attempts to evaluate the Python script using `PyRun_String()`, handling
 * various return types including `None`, `bool`, `int`, and `str`. If an error occurs during
 * execution, it logs the error and returns an error result.
 *
 * Possible return types:
 * - `Python::Types::Integer` for integer values.
 * - `Python::Types::Boolean` for boolean values.
 * - `Python::Types::String` for string values.
 * - `Python::Types::Error` if an error occurs or an unsupported type is encountered.
 */
MILLENNIUM const Python::EvalResult EvaluatePython(std::string pluginName, std::string script) 
{
    PyObject* mainModule = PyImport_AddModule("__main__");

    if (!mainModule) 
    {
        const auto message = fmt::format("Failed to fetch python module on [{}]. This usually means the GIL could not be acquired either because the backend froze or crashed", pluginName);

        ErrorToLogger(pluginName, message);
        return { message, Python::Types::Error };
    }

    PyObject* globalDictionaryObj = PyModule_GetDict(mainModule);
    PyObject* EvaluatedObj = PyRun_String(script.c_str(), Py_eval_input, globalDictionaryObj, globalDictionaryObj);

    if (!EvaluatedObj && PyErr_Occurred()) 
    {
        const auto [errorMessage, traceback] = Python::ActiveExceptionInformation();

        Logger.PrintMessage(" FFI-ERROR ", fmt::format("Failed to call {}: {}\n{}{}", script, COL_RED, traceback, COL_RESET), COL_RED);
    }

    if (EvaluatedObj == nullptr || EvaluatedObj == Py_None) 
    {
        return { "0", Python::Types::Integer }; // whitelist NoneType as integer 0
    }
    if (PyBool_Check(EvaluatedObj)) 
    {
        return { PyLong_AsLong(EvaluatedObj) == 0 ? "False" : "True", Python::Types::Boolean };
    }
    else if (PyLong_Check(EvaluatedObj)) 
    {
        return { std::to_string(PyLong_AsLong(EvaluatedObj)), Python::Types::Integer };
    }
    else if (PyUnicode_Check(EvaluatedObj)) 
    {
        return { PyUnicode_AsUTF8(EvaluatedObj), Python::Types::String };
    }
    else if (PyDict_Check(EvaluatedObj) || PyList_Check(EvaluatedObj) || PyTuple_Check(EvaluatedObj)) 
    {
        // Attempt to serialize to JSON string
        auto jsonStrOpt = PyObjectToJsonString(EvaluatedObj);
        if (jsonStrOpt.has_value()) 
        {
            return { jsonStrOpt.value(), Python::Types::JSON }; // Return JSON as string type
        }
        else 
        {
            // Fallback: failed JSON serialization
            PyObject* objectType = PyObject_Type(EvaluatedObj);
            PyObject* strObjectType = PyObject_Str(objectType);
            const char* strTypeName = PyUnicode_AsUTF8(strObjectType);
            PyErr_Clear();
            Py_XDECREF(objectType);
            Py_XDECREF(strObjectType);
            return { fmt::format("Failed to serialize return value of type [{}] to JSON", strTypeName), Python::Types::Error };
        }
    }

    PyObject* objectType = PyObject_Type(EvaluatedObj);
    PyObject* strObjectType = PyObject_Str(objectType);
    const char* strTypeName = PyUnicode_AsUTF8(strObjectType);
    PyErr_Clear();  // Clear any Python exception
    Py_XDECREF(objectType);
    Py_XDECREF(strObjectType);

    return { fmt::format("Millennium expected return type [int, str, bool, JSON-serializable dict/list/tuple] but received [{}]", strTypeName) , Python::Types::Error };
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
    Python::EvalResult response = InvokeMethodSync(functionCall, globalDictionaryObj);

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
