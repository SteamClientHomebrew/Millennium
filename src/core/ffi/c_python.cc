#include "ffi.h"
#include <core/py_controller/co_spawn.h>
#include <iostream>
#include <tuple>
#include <core/py_hooks/logger.h>

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

std::tuple<std::string, std::string> Python::GetExceptionInformaton() 
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
        const auto [errorMessage, traceback] = Python::GetExceptionInformaton();

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

Python::EvalResult Python::LockGILAndEvaluate(std::string pluginName, std::string script)
{
    auto [strPluginName, threadState, interpMutex] = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

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

void Python::LockGILAndDiscardEvaluate(std::string pluginName, std::string script)
{
    auto [strPluginName, threadState, interpMutex] = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

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
            const auto [errorMessage, traceback] = Python::GetExceptionInformaton();
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
