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

#include "__builtins__/ipc_handler.h"
#include <functional>
#include <internal_logger.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

std::unordered_map<std::string, std::any>& GetCoreExports()
{
    static std::unordered_map<std::string, std::any> instance;
    return instance;
}

nlohmann::json HandleIpcMessage(const std::string& function_name, const nlohmann::json& args)
{
    /** Not applicable to this plugin, so skip it */
    if (function_name == "__builtins__.__millennium_plugin_settings_parser__")
        throw std::runtime_error("Not applicable to this plugin");

    auto it = GetCoreExports().find(function_name);
    if (it != GetCoreExports().end()) {
        using FuncType = std::function<nlohmann::json(const nlohmann::json&)>;
        if (it->second.type() == typeid(FuncType)) {
            auto func = std::any_cast<FuncType>(it->second);
            return func(args);
        }
    }

    LOG_ERROR("Function not found or invalid type: {}", function_name);
    throw std::runtime_error("Function not found or invalid type: " + function_name);
}