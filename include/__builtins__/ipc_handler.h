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

extern std::unordered_map<std::string, std::any> __CORE_EXPORTS__;

#define MILLENNIUM_IPC_DECL(name)                                                                                                                                                  \
    nlohmann::json _impl_##name(const nlohmann::json& ARGS);                                                                                                                       \
    struct _##name##_registrar                                                                                                                                                     \
    {                                                                                                                                                                              \
        _##name##_registrar()                                                                                                                                                      \
        {                                                                                                                                                                          \
            __CORE_EXPORTS__[#name] = std::function<nlohmann::json(const nlohmann::json&)>([](const nlohmann::json& j) -> nlohmann::json                                           \
            {                                                                                                                                                                      \
                if constexpr (std::is_same_v<decltype(_impl_##name(j)), void>) {                                                                                                   \
                    _impl_##name(j);                                                                                                                                               \
                    return {};                                                                                                                                                     \
                } else {                                                                                                                                                           \
                    return _impl_##name(j);                                                                                                                                        \
                }                                                                                                                                                                  \
            });                                                                                                                                                                    \
        }                                                                                                                                                                          \
    };                                                                                                                                                                             \
    static _##name##_registrar _##name##_registrar_instance;                                                                                                                       \
    nlohmann::json _impl_##name(const nlohmann::json& ARGS)

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

nlohmann::json HandleIpcMessage(const std::string& function_name, const nlohmann::json& args);