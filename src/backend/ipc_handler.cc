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

#include "head/ipc_handler.h"
#include "millennium/ffi.h"
#include "millennium/logger.h"

/**
 * Stored in a static instance only instantiated after it's called once.
 * This avoids static initialization order issues.
 */
std::unordered_map<std::string, std::any>& GetCoreExports()
{
    static std::unordered_map<std::string, std::any> instance;
    return instance;
}

/**
 * @brief Handle IPC messages from Millennium frontend
 * @param function_name Name of the function to call
 * @param args JSON arguments to pass to the function. Args must be an object, mapping kwargs to values.
 */
nlohmann::ordered_json HandleIpcMessage(const std::string& function_name, const nlohmann::json& args)
{
    // Skip plugin settings parser as it's not applicable
    if (function_name == "__builtins__.__millennium_plugin_settings_parser__") {
        throw std::runtime_error("Not applicable to this plugin");
    }

    const auto& exports = GetCoreExports();
    const auto it = exports.find(function_name);

    if (it == exports.end()) {
        const std::string errorMsg = "Function not found: " + function_name;
        LOG_ERROR("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    using FuncType = std::function<nlohmann::ordered_json(const nlohmann::ordered_json&)>;
    if (it->second.type() != typeid(FuncType)) {
        const std::string errorMsg = "Invalid function type: " + function_name;
        LOG_ERROR("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    /** call the functions with provided args */
    return (std::any_cast<FuncType>(it->second))(args);
}

void IpcForwardInstallLog(const InstallMessage& message)
{
    const std::vector<JavaScript::JsFunctionConstructTypes> params = {
        { message.status, JavaScript::Types::String },
        { fmt::format("{}", message.progress), JavaScript::Types::Integer },
        { fmt::format("{}", message.isComplete), JavaScript::Types::Boolean }
    };

    JavaScript::ExecuteOnSharedJsContext(JavaScript::ConstructFunctionCall("core", "InstallerMessageEmitter", params));
}
