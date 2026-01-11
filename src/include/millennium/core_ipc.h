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
#include "millennium/ffi.h"
#include "millennium/sysfs.h"
#include "millennium/types.h"

class ipc_main
{
  public:
    ipc_main(std::shared_ptr<SettingsStore> settings_store) : m_settings_store(std::move(settings_store))
    {
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

    json process_message(json payload);

  private:
    json call_server_method(const json& call);
    json on_front_end_loaded(const json& call);
    json call_frontend_method(const json& call);

    EvalResult handle_plugin_server_method(const std::string& pluginName, const json& message);
    EvalResult handle_core_server_method(const json& call);

    std::shared_ptr<SettingsStore> m_settings_store;
};