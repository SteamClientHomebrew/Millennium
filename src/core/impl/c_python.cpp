#include "mutex_impl.hpp"
#include <core/backend/co_spawn.hpp>
#include <iostream>

std::string python::construct_fncall(nlohmann::basic_json<> data)
{
    std::string out = data["methodName"];
    out += "(";

    if (data.contains("argumentList"))
    {
        int index = 0, total_items = data["argumentList"].size();

        for (auto it = data["argumentList"].begin(); it != data["argumentList"].end(); ++it, ++index)
        {
            auto& key = it.key();
            auto& value = it.value();

            if (value.is_boolean()) { out += fmt::format("{}={}", key, value ? "True" : "False"); }
            else { out += fmt::format("{}={}", key, value.dump()); }

            if (index < total_items - 1) {
                out += ", ";
            }
        }
    }
    out += ")";
    return out;
}

const python::return_t lock_gil(std::string plugin_name, std::string script) 
{
    // Execute the script and assign the return value to a variable
    PyObject* global_dict = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* rv_object = PyRun_String(script.c_str(), Py_eval_input, global_dict, global_dict);

    // An exception occurred
    if (!rv_object && PyErr_Occurred()) {

        PyObject* ptype, * pvalue, * ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

        PyObject* pStrErrorMessage = PyObject_Str(pvalue);
        if (pStrErrorMessage) {

            const char* errorMessage = PyUnicode_AsUTF8(pStrErrorMessage);
            if (errorMessage) {
                console.err(fmt::format("[interop-dispatch] Error calling backend function: {}", errorMessage));
            }
            return { errorMessage, python::type_t::_err };
        }
    }

    if (rv_object == nullptr || rv_object == Py_None) {
        return { "0", python::type_t::_int }; // whitelist NoneType
    }
    if (PyBool_Check(rv_object)) {
        return { PyLong_AsLong(rv_object) == 0 ? "False" : "True", python::type_t::_bool };
    }
    else if (PyLong_Check(rv_object)) {
        return { std::to_string(PyLong_AsLong(rv_object)), python::type_t::_int };
    }
    else if (PyUnicode_Check(rv_object)) {
        return { PyUnicode_AsUTF8(rv_object), python::type_t::_string };
    }

    // Print the typename
    PyObject* object_type = PyObject_Type(rv_object);
    PyObject* object_type_str = PyObject_Str(object_type);
    const char* type_name = PyUnicode_AsUTF8(object_type_str);
    PyErr_Clear();  // Clear any Python exception
    Py_XDECREF(object_type);
    Py_XDECREF(object_type_str);

    return { fmt::format("Millennium expected return type [int, str, bool] but recevied [{}]", type_name) , python::type_t::_err };
}

python::return_t python::evaluate_lock(std::string plugin_name, std::string script)
{
    auto [name, state, _auxts] = plugin_manager::get().get_thread_state(plugin_name);

    if (state == nullptr) {
        console.err(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
        return { "overstepped partying thread state", _err };
    }

    PyThreadState* auxts = PyThreadState_New(PyInterpreterState_Main());
    PyEval_RestoreThread(auxts);

    // switch global lock to current thread swap
    PyGILState_STATE gstate = PyGILState_Ensure();

    // switch thread state to respective backend 
    PyThreadState_Swap(state);

    if (state == NULL) {
        console.err("script execution was queried but the receiving parties thread state was nullptr");
        return { "thread state was nullptr", _err };
    }

    python::return_t response = lock_gil(plugin_name, script);

    PyThreadState_Clear(auxts);
    PyThreadState_Swap(auxts);

    PyGILState_Release(gstate);
    PyThreadState_DeleteCurrent();
    return response;
}

void python::run_lock(std::string plugin_name, std::string script)
{
    auto [name, state, _] = plugin_manager::get().get_thread_state(plugin_name);

    if (state == nullptr || state == NULL) {
        console.err(fmt::format("couldn't get thread state ptr from plugin [{}], maybe it crashed or exited early? ", plugin_name));
        return;
    }
    const auto interp_state = PyInterpreterState_Main();

    PyThreadState* auxts = PyThreadState_New(interp_state);
    PyEval_RestoreThread(auxts);

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyThreadState_Swap(state);

    // PyRun_SimpleString(fmt::format(
    //     "try:\n\t{}\nexcept Exception as e:\n\tprint(f'[Millennium] An error occured running {} lock: {{e}}')", 
    //     script, script
    // ).c_str());
    
    // Get the current frame
    PyRun_SimpleString(script.c_str());

    PyThreadState_Clear(auxts);
    PyThreadState_Swap(auxts);

    PyGILState_Release(gstate);
    PyThreadState_DeleteCurrent();
}
