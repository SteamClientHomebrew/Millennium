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

/**
 * @brief Create a semantically correct JavaScript function call from a JSON object.
 * 
 * @param data The JSON object containing the function call information.
 * - methodName: The name of the method to call.
 * - argumentList: A list of arguments to pass to the function.
 * 
 * @return The constructed function call.
 */
std::string Python::ConstructFunctionCall(nlohmann::basic_json<> data)
{
    std::string strFunctionCall = data["methodName"];
    strFunctionCall += "(";

    if (data.contains("argumentList"))
    {
        int index = 0, argumentLength = data["argumentList"].size();

        for (auto argIterator = data["argumentList"].begin(); argIterator != data["argumentList"].end(); ++argIterator, ++index)
        {
            auto& parameterKeyWord = argIterator.key();
            auto& parameterData = argIterator.value();

            if (parameterData.is_boolean()) 
            { 
                strFunctionCall += fmt::format("{}={}", parameterKeyWord, parameterData ? "True" : "False"); 
            }
            else 
            { 
                strFunctionCall += fmt::format("{}={}", parameterKeyWord, parameterData.dump()); 
            }

            if (index < argumentLength - 1)
            {
                strFunctionCall += ", ";
            }
        }
    }
    strFunctionCall += ")";
    return strFunctionCall;
}

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
const Python::EvalResult EvaluatePython(std::string pluginName, std::string script) 
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
        return { "0", Python::Types::Integer }; // whitelist NoneType
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

    PyObject* objectType = PyObject_Type(EvaluatedObj);
    PyObject* strObjectType = PyObject_Str(objectType);
    const char* strTypeName = PyUnicode_AsUTF8(strObjectType);
    PyErr_Clear();  // Clear any Python exception
    Py_XDECREF(objectType);
    Py_XDECREF(strObjectType);

    return { fmt::format("Millennium expected return type [int, str, bool] but received [{}]", strTypeName) , Python::Types::Error };
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
Python::EvalResult Python::LockGILAndEvaluate(std::string pluginName, std::string script)
{
    auto& [strPluginName, threadState, interpMutex] = *PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (threadState == nullptr) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? Tried to evaluate ->\n{}", pluginName, script));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", script));

        return { "overstepped partying thread state", Error };
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);

    if (threadState == NULL) 
    {
        LOG_ERROR("script execution was queried but the receiving parties thread state was nullptr");
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script: {}", script));

        return { "thread state was nullptr", Error };
    }

    Python::EvalResult response = EvaluatePython(pluginName, script);

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
void Python::LockGILAndDiscardEvaluate(std::string pluginName, std::string script)
{
    auto& [strPluginName, threadState, interpMutex] = *PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (threadState == nullptr) 
    {
        LOG_ERROR(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? Tried to evaluate ->\n{}", pluginName, script));
        ErrorToLogger(pluginName, fmt::format("Failed to evaluate script; maybe it crashed or exited early?: {}", script));
        return;
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);
    {
        PyObject* globalDictionaryObj = PyModule_GetDict(PyImport_AddModule("__main__"));
        PyObject* EvaluatedObj = PyRun_String(script.c_str(), Py_eval_input, globalDictionaryObj, globalDictionaryObj);

        if (!EvaluatedObj && PyErr_Occurred()) 
        {
            const auto [errorMessage, traceback] = Python::ActiveExceptionInformation();
            PyErr_Clear();

            if (errorMessage == "name 'plugin' is not defined")
            {
                Logger.PrintMessage(" FFI-ERROR ", fmt::format("Millennium failed to call {} on {} as the function "
                    "does not exist, or the interpreter crashed before it was loaded.", script, pluginName), COL_RED);

                ErrorToLogger(pluginName, fmt::format("Millennium failed to call {} on {} as the function "
                    "does not exist, or the interpreter crashed before it was loaded.", script, pluginName));
                return;
            }

            Logger.PrintMessage(" FFI-ERROR ", fmt::format("Millennium failed to call {} on {}: {}\n{}{}", script, pluginName, COL_RED, traceback, COL_RESET), COL_RED);
            ErrorToLogger(pluginName, fmt::format("Millennium failed to call {} on {}: {}\n", script, pluginName, traceback));
        }
    }
    pythonGilLock->ReleaseAndUnLockGIL();
}
