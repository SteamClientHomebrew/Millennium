#include "ffi.h"
#include <core/py_controller/co_spawn.h>
#include <iostream>

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

const Python::EvalResult EvaluatePython(std::string pluginName, std::string script) 
{
    PyObject* globalDictionaryObj = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* EvaluatedObj = PyRun_String(script.c_str(), Py_eval_input, globalDictionaryObj, globalDictionaryObj);

    if (!EvaluatedObj && PyErr_Occurred()) 
    {
        PyObject* typeObj, *valueObj, *traceBackObj;
        PyErr_Fetch(&typeObj, &valueObj, &traceBackObj);
        PyErr_NormalizeException(&typeObj, &valueObj, &traceBackObj);

        PyObject* pStrErrorMessage = PyObject_Str(valueObj);
        if (pStrErrorMessage) 
        {
            const char* errorMessage = PyUnicode_AsUTF8(pStrErrorMessage);

            if (errorMessage) 
            {
                Logger.Error(fmt::format("[interop-dispatch] Error calling backend function: {}", errorMessage));
                return { errorMessage, Python::ReturnTypes::Error };
            }
            else 
            {
                return { "unknown error", Python::ReturnTypes::Error};
            }
        }
    }

    if (EvaluatedObj == nullptr || EvaluatedObj == Py_None) 
    {
        return { "0", Python::ReturnTypes::Integer }; // whitelist NoneType
    }
    if (PyBool_Check(EvaluatedObj)) 
    {
        return { PyLong_AsLong(EvaluatedObj) == 0 ? "False" : "True", Python::ReturnTypes::Boolean };
    }
    else if (PyLong_Check(EvaluatedObj)) 
    {
        return { std::to_string(PyLong_AsLong(EvaluatedObj)), Python::ReturnTypes::Integer };
    }
    else if (PyUnicode_Check(EvaluatedObj)) 
    {
        return { PyUnicode_AsUTF8(EvaluatedObj), Python::ReturnTypes::String };
    }

    PyObject* objectType = PyObject_Type(EvaluatedObj);
    PyObject* strObjectType = PyObject_Str(objectType);
    const char* strTypeName = PyUnicode_AsUTF8(strObjectType);
    PyErr_Clear();  // Clear any Python exception
    Py_XDECREF(objectType);
    Py_XDECREF(strObjectType);

    return { fmt::format("Millennium expected return type [int, str, bool] but received [{}]", strTypeName) , Python::ReturnTypes::Error };
}

Python::EvalResult Python::LockGILAndEvaluate(std::string pluginName, std::string script)
{
    auto [strPluginName, threadState] = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (threadState == nullptr) 
    {
        Logger.Error(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", pluginName));
        return { "overstepped partying thread state", Error };
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);

    if (threadState == NULL) 
    {
        Logger.Error("script execution was queried but the receiving parties thread state was nullptr");
        return { "thread state was nullptr", Error };
    }

    Python::EvalResult response = EvaluatePython(pluginName, script);

    pythonGilLock->ReleaseAndUnLockGIL();
    return response;
}

void Python::LockGILAndDiscardEvaluate(std::string pluginName, std::string script)
{
    auto [strPluginName, threadState] = PythonManager::GetInstance().GetPythonThreadStateFromName(pluginName);

    if (threadState == nullptr) 
    {
        Logger.Error(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", pluginName));
        return;
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(threadState);
    {
        PyRun_SimpleString(script.c_str());
    }
    pythonGilLock->ReleaseAndUnLockGIL();
}
