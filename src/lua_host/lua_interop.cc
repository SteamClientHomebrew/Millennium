/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include "millennium/core_ipc.h"
#include "millennium/logger.h"

std::string ipc_main::get_vm_call_result_error(vm_call_result result)
{
    nlohmann::json j;
    std::visit([&j](auto&& arg)
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            j = nullptr;
        } else {
            j = arg;
        }
    }, result.value);
    return j.dump();
}

ipc_main::vm_call_result ipc_main::lua_evaluate(std::string pluginName, nlohmann::json script)
{
    const auto shared_backend_mgr = m_backend_manager.lock();
    if (!shared_backend_mgr) {
        return { false, std::string("backend_manager is no longer available") };
    }

    auto result = shared_backend_mgr->evaluate(pluginName, script);

    if (result.contains("success") && result["success"].is_boolean() && result["success"].get<bool>()) {
        vm_call_result vm_result;
        vm_result.success = true;

        if (result.contains("value") && !result["value"].is_null()) {
            const auto& val = result["value"];
            if (val.is_string()) {
                /*
                 * lua backends return JSON-encoded strings for tables/objects.
                 * parse them into nlohmann::json so the return type is consistent
                 * with core C++ methods (which return raw JSON values).
                 */
                auto str = val.get<std::string>();
                auto parsed = nlohmann::ordered_json::parse(str, nullptr, false);
                if (!parsed.is_discarded())
                    vm_result.value = std::move(parsed);
                else
                    vm_result.value = str;
            } else if (val.is_boolean())
                vm_result.value = val.get<bool>();
            else if (val.is_number_float())
                vm_result.value = val.get<double>();
            else if (val.is_number_integer())
                vm_result.value = val.get<int64_t>();
            else
                vm_result.value = std::monostate{};
        } else {
            vm_result.value = std::monostate{};
        }

        return vm_result;
    }

    std::string error = result.value("error", "unknown error from child process");
    return { false, error };
}

void ipc_main::lua_call_frontend_loaded(std::string pluginName)
{
    const auto shared_backend_mgr = m_backend_manager.lock();
    if (!shared_backend_mgr) {
        LOG_ERROR("Failed to lock backend manager");
        return;
    }

    shared_backend_mgr->notify_frontend_loaded(pluginName);
}
