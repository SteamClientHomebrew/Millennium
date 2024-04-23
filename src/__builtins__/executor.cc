#include "executor.h"
#include <core/backend/co_spawn.hpp>
#include <core/impl/mutex_impl.hpp>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fstream>
#include <utilities/log.hpp>
#include <generic/stream_parser.h>
#include <generic/base.h>
#include <core/hooks/web_load.h>

PyObject* py_get_user_settings(PyObject* self, PyObject* args)
{
    const auto data = stream_buffer::file::readJsonSync(stream_buffer::steam_path().string());
    PyObject* py_dict = PyDict_New();

    for (auto it = data.begin(); it != data.end(); ++it) 
    {
        PyObject* key = PyUnicode_FromString(it.key().c_str());
        PyObject* value = PyUnicode_FromString(it.value().get<std::string>().c_str());

        PyDict_SetItem(py_dict, key, value);
        Py_DECREF(key);
        Py_DECREF(value);
    }

    return py_dict;
}

PyObject* py_set_user_settings_key(PyObject* self, PyObject* args)
{
    const char* key;
    const char* value;

    if (!PyArg_ParseTuple(args, "ss", &key, &value)) 
    {
        return NULL;
    }

    auto data = stream_buffer::file::readJsonSync(stream_buffer::steam_path().string());
    data[key] = value;

    stream_buffer::file::writeFileSync(stream_buffer::steam_path().string(), data.dump(4)); 
    Py_RETURN_NONE;
}

PyObject* call_frontend_method(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* method_name = NULL;
    PyObject* params_list = NULL;

    static const char* kwlist[] = { "method_name", "params", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", (char**)kwlist, &method_name, &params_list)) {
        return NULL;
    }

    std::vector<javascript::param_types> params;

    if (params_list != NULL)
    {
        if (!PyList_Check(params_list)) {
            PyErr_SetString(PyExc_TypeError, "params must be a list");
            return NULL;
        }

        Py_ssize_t list_size = PyList_Size(params_list);
        for (Py_ssize_t i = 0; i < list_size; ++i) {
            PyObject* item = PyList_GetItem(params_list, i);
            const char* value_str = PyUnicode_AsUTF8(PyObject_Str(item));
            const char* value_type = Py_TYPE(item)->tp_name;

            if (strcmp(value_type, "str") != 0 && strcmp(value_type, "bool") != 0 && strcmp(value_type, "int") != 0) {
                PyErr_SetString(PyExc_TypeError, "Millennium's IPC can only handle [bool, str, int]");
                return NULL;
            }

            params.push_back({ value_str, value_type });
        }
    }

    PyObject* globals = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyObject* result = PyRun_String("MILLENNIUM_PLUGIN_SECRET_NAME", Py_eval_input, globals, globals);

    if (result == nullptr || PyErr_Occurred()) {
        console.err("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return NULL;
    }

    std::string script = javascript::construct_fncall(PyUnicode_AsUTF8(PyObject_Str(result)), method_name, params);

    return javascript::evaluate_lock(script);
}

PyObject* py_get_millennium_version(PyObject* self, PyObject* args) { return PyUnicode_FromString(g_mversion); }
PyObject* py_steam_path(PyObject* self, PyObject* args) { return PyUnicode_FromString(stream_buffer::steam_path().string().c_str()); }


PyObject* remove_browser_module(PyObject* self, PyObject* args) { 
    int js_index;

    if (!PyArg_ParseTuple(args, "i", &js_index)) {
        return NULL;
    }

    auto list = webkit_handler::get().h_list_ptr;

    bool success = false;

    for (auto it = (*list).begin(); it != (*list).end();) 
    {
        if (it->id == js_index) 
        {
            it = list->erase(it);
            success = true;
        } 
        else ++it;
    }
    return PyBool_FromLong(success);
}

int add_module(PyObject* args, webkit_handler::type_t type) {
    const char* item_str;

    if (!PyArg_ParseTuple(args, "s", &item_str)) {
        return 0;
    }

    hook_tag++;

    auto path = stream_buffer::steam_path() / "steamui" / item_str;

    webkit_handler::get().h_list_ptr->push_back({ path.generic_string(), type, hook_tag });
    return hook_tag;
}

PyObject* add_browser_css(PyObject* self, PyObject* args) 
{ 
    return PyLong_FromLong(add_module(args, webkit_handler::type_t::STYLESHEET));
}

PyObject* add_browser_js(PyObject* self, PyObject* args) 
{ 
    return PyLong_FromLong(add_module(args, webkit_handler::type_t::JAVASCRIPT));
}

PyMethodDef* get_module_handle()
{
    static PyMethodDef module_methods[] = 
    {
        { "add_browser_css", add_browser_css, METH_VARARGS, NULL },
        { "add_browser_js", add_browser_js, METH_VARARGS, NULL },
        { "remove_browser_module", remove_browser_module, METH_VARARGS, NULL },

        { "get_user_settings", py_get_user_settings, METH_NOARGS, NULL },
        { "set_user_settings_key", py_set_user_settings_key, METH_VARARGS, NULL },
        { "version", py_get_millennium_version, METH_NOARGS, NULL },
        { "steam_path", py_steam_path, METH_NOARGS, NULL },
        { "call_frontend_method", (PyCFunction)call_frontend_method, METH_VARARGS | METH_KEYWORDS, NULL },
        {NULL, NULL, 0, NULL} // Sentinel
    };

    return module_methods;
}
