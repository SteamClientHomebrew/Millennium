#include "ffi.hpp"
#include <core/py_controller/co_spawn.hpp>
#include <iostream>

std::string Python::ConstructFunctionCall(nlohmann::basic_json<> data)
{
    std::string strFunctionCall = data["methodName"];
    strFunctionCall += "(";

    if (data.contains("argumentList"))
    {
        int index = 0, argumentLength = data["argumentList"].size();

        for (auto it = data["argumentList"].begin(); it != data["argumentList"].end(); ++it, ++index)
        {
            auto& key = it.key();
            auto& value = it.value();

            if (value.is_boolean()) 
            { 
                strFunctionCall += fmt::format("{}={}", key, value ? "True" : "False"); 
            }
            else 
            { 
                strFunctionCall += fmt::format("{}={}", key, value.dump()); 
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

const Python::EvalResult EvaluatePython(std::string plugin_name, std::string script) 
{
    // Execute the script and assign the return value to a variable
    PyObject* global_dict = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* rv_object = PyRun_String(script.c_str(), Py_eval_input, global_dict, global_dict);

    // An exception occurred
    if (!rv_object && PyErr_Occurred()) 
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

    if (rv_object == nullptr || rv_object == Py_None) 
    {
        return { "0", Python::ReturnTypes::Integer }; // whitelist NoneType
    }
    if (PyBool_Check(rv_object)) 
    {
        return { PyLong_AsLong(rv_object) == 0 ? "False" : "True", Python::ReturnTypes::Boolean };
    }
    else if (PyLong_Check(rv_object)) 
    {
        return { std::to_string(PyLong_AsLong(rv_object)), Python::ReturnTypes::Integer };
    }
    else if (PyUnicode_Check(rv_object)) 
    {
        return { PyUnicode_AsUTF8(rv_object), Python::ReturnTypes::String };
    }

    PyObject* objectType = PyObject_Type(rv_object);
    PyObject* strObjectType = PyObject_Str(objectType);
    const char* strTypeName = PyUnicode_AsUTF8(strObjectType);
    PyErr_Clear();  // Clear any Python exception
    Py_XDECREF(objectType);
    Py_XDECREF(strObjectType);

    return { fmt::format("Millennium expected return type [int, str, bool] but received [{}]", strTypeName) , Python::ReturnTypes::Error };
}

Python::EvalResult Python::LockGILAndEvaluate(std::string plugin_name, std::string script)
{
    auto [pluginName, threadState] = PythonManager::GetInstance().GetPythonThreadStateFromName(plugin_name);

    if (threadState == nullptr) 
    {
        Logger.Error(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
        return { "overstepped partying thread state", Error };
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();

    pythonGilLock->HoldAndLockGILOnThread(threadState);

    if (threadState == NULL) {
        Logger.Error("script execution was queried but the receiving parties thread state was nullptr");
        return { "thread state was nullptr", Error };
    }

    Python::EvalResult response = EvaluatePython(plugin_name, script);

    pythonGilLock->ReleaseAndUnLockGIL();
    return response;
}

void Python::LockGILAndDiscardEvaluate(std::string plugin_name, std::string script)
{
    auto [pluginName, state] = PythonManager::GetInstance().GetPythonThreadStateFromName(plugin_name);

    if (state == nullptr) 
    {
        Logger.Error(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
        return;
    }

    std::shared_ptr<PythonGIL> pythonGilLock = std::make_shared<PythonGIL>();
    pythonGilLock->HoldAndLockGILOnThread(state);

    PyRun_SimpleString(script.c_str());

    pythonGilLock->ReleaseAndUnLockGIL();
}
