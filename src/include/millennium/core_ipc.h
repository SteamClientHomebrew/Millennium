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

#pragma once
#include "millennium/backend_mgr.h"
#include "millennium/cdp_api.h"
#include "millennium/logger.h"
#include "millennium/sysfs.h"
#include "millennium/types.h"

#include <Python.h>
#include <lua.hpp>

class ipc_main
{
  public:
    ipc_main(std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<cdp_client> cdp_client, std::shared_ptr<backend_manager> manager)
        : m_settings_store(std::move(settings_store)), m_cdp(std::move(cdp_client)), m_backend_manager(std::move(manager))
    {
    }

    ~ipc_main()
    {
        Logger.Log("Successfully shut down ipc_main...");
    }

    enum Builtins
    {
        CALL_SERVER_METHOD,
        FRONT_END_LOADED,
        CALL_FRONTEND_METHOD
    };

    enum ErrorType
    {
        AUTHENTICATION_ERROR,
        INTERNAL_ERROR
    };

    enum class javascript_evaluation_type
    {
        Boolean,
        String,
        Integer
    };

    enum class FFI_Type
    {
        Boolean,
        String,
        JSON,
        Integer,
        Error,
        UnknownType
    };

    struct vm_call_result
    {
        FFI_Type type;
        std::string plain;
    };

    vm_call_result lua_evaluate(std::string pluginName, nlohmann::json script);
    void lua_call_frontend_loaded(std::string pluginName);

    vm_call_result python_evaluate(std::string pluginName, nlohmann::json script);
    void python_call_frontend_loaded(std::string pluginName);

    static inline const std::unordered_map<nlohmann::json::value_t, FFI_Type> FFIMap_t = {
        { nlohmann::json::value_t::boolean,         FFI_Type::Boolean },
        { nlohmann::json::value_t::string,          FFI_Type::String  },
        { nlohmann::json::value_t::number_integer,  FFI_Type::Integer },
        { nlohmann::json::value_t::number_unsigned, FFI_Type::Integer },
        { nlohmann::json::value_t::object,          FFI_Type::JSON    },
        { nlohmann::json::value_t::array,           FFI_Type::JSON    }
    };

    static inline const std::map<std::string, javascript_evaluation_type> str_type_map = {
        { "string",  javascript_evaluation_type::String  },
        { "boolean", javascript_evaluation_type::Boolean },
        { "number",  javascript_evaluation_type::Integer }
    };

    struct javascript_parameter
    {
        std::string value;
        javascript_evaluation_type type;
    };

    class javascript_evaluation_result
    {
        std::tuple<json /** payload */, bool /** success */> response;
        bool valid;
        std::string error;

      public:
        javascript_evaluation_result(std::tuple<json, bool> res, bool v, std::string err = "") : response(std::move(res)), valid(v), error(std::move(err))
        {
        }

        PyObject* to_python() const;
        int to_lua(lua_State* L) const;
        json to_json(const std::string& pluginName) const;
    };

    javascript_evaluation_result evaluate_javascript_expression(std::string script);
    const std::string compile_javascript_expression(std::string plugin, std::string methodName, std::vector<javascript_parameter> fnParams);

    json process_message(json payload);

  private:
    json call_server_method(const json& call);
    json on_front_end_loaded(const json& call);
    json call_frontend_method(const json& call);

    vm_call_result handle_plugin_server_method(const std::string& pluginName, const json& message);
    vm_call_result handle_core_server_method(const json& call);

    std::shared_ptr<SettingsStore> m_settings_store;
    std::shared_ptr<cdp_client> m_cdp;
    std::shared_ptr<backend_manager> m_backend_manager;
};
