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

#include <any>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::any>& GetCoreExports();

#define MILLENNIUM_IPC_DECL(name)                                                                                                                                                  \
    nlohmann::ordered_json impl##name(const nlohmann::ordered_json& ARGS);                                                                                                         \
    struct name##registrar                                                                                                                                                         \
    {                                                                                                                                                                              \
        name##registrar()                                                                                                                                                          \
        {                                                                                                                                                                          \
            GetCoreExports()[#name] = std::function<nlohmann::ordered_json(const nlohmann::ordered_json&)>([](const nlohmann::ordered_json& j) -> nlohmann::ordered_json           \
            {                                                                                                                                                                      \
                try {                                                                                                                                                              \
                    if constexpr (std::is_same_v<decltype(impl##name(j)), void>) {                                                                                                 \
                        impl##name(j);                                                                                                                                             \
                        return {};                                                                                                                                                 \
                    } else {                                                                                                                                                       \
                        return impl##name(j);                                                                                                                                      \
                    }                                                                                                                                                              \
                } catch (const nlohmann::ordered_json::exception& e) {                                                                                                             \
                    LOG_ERROR("Failed to call {} - nlohmann::ordered_json::exception: {}", #name, e.what());                                                                       \
                    return {};                                                                                                                                                     \
                } catch (...) {                                                                                                                                                    \
                    LOG_ERROR("Failed to call {}", #name);                                                                                                                         \
                    return {};                                                                                                                                                     \
                }                                                                                                                                                                  \
            });                                                                                                                                                                    \
        }                                                                                                                                                                          \
    };                                                                                                                                                                             \
    static name##registrar name##registrar_instance;                                                                                                                               \
    nlohmann::ordered_json impl##name([[maybe_unused]] const nlohmann::ordered_json& ARGS)

/** ffi function that returns a value */
#define IPC_RET(name, expr)                                                                                                                                                        \
    MILLENNIUM_IPC_DECL(name)                                                                                                                                                      \
    {                                                                                                                                                                              \
        return expr;                                                                                                                                                               \
    }

/** ffi function that returns nothing */
#define IPC_NIL(name, expr)                                                                                                                                                        \
    MILLENNIUM_IPC_DECL(name)                                                                                                                                                      \
    {                                                                                                                                                                              \
        expr;                                                                                                                                                                      \
        return {};                                                                                                                                                                 \
    }

nlohmann::ordered_json HandleIpcMessage(const std::string& function_name, const nlohmann::json& args);

struct InstallMessage
{
    std::string status;
    double progress;
    bool isComplete;
};

void IpcForwardInstallLog(const InstallMessage& message);
