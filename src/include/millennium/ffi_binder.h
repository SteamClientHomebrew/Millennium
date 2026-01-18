/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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
#include "millennium/core_ipc.h"
#include "millennium/cdp_api.h"

namespace ffi_constants
{
static const char* const binding_name = "__private_millennium_ffi_do_not_use__";
static const char* const frontend_binding_name = "MILLENNIUM_PRIVATE_INTERNAL_FOREIGN_FUNCTION_INTERFACE_DO_NOT_USE";
} // namespace ffi_constants

class ffi_binder
{
  public:
    ffi_binder(std::shared_ptr<cdp_client> client, std::shared_ptr<settings_store> settings_store, std::shared_ptr<ipc_main> ipc_main);
    ~ffi_binder();

  private:
    std::shared_ptr<cdp_client> m_client;
    std::shared_ptr<settings_store> m_settings_store;
    std::shared_ptr<ipc_main> m_ipc_main;

    void callback_into_js(const json params, const int request_id, json result);

    /**
     * event handler for Runtime.bindingCalled events
     */
    void binding_call_hdlr(const json& params);
    void execution_ctx_created_hdlr(const json& params);

    bool is_valid_request(const json& params);
};
