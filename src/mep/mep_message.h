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

#pragma once

#include <functional>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace mep
{
struct client_context
{
    std::function<bool(const nlohmann::json& event)> push;
    std::function<std::string(std::function<void()> cancel_fn)> subscribe;
    std::function<bool(const std::string& subscription_id)> unsubscribe;
};

struct request_t
{
    std::string id;                  /* caller-assigned correlation ID, echoed verbatim in the response. */
    std::string method;              /* method name, e.g. "plugin.list" or "plugin.start". */
    nlohmann::json params = nullptr; /* method-specific parameters; null if the method takes none. */

    static std::optional<request_t> from_json(const nlohmann::json& j) noexcept;
    nlohmann::json to_json() const;
};

struct response_t
{
    std::string id;
    nlohmann::json result;
    std::optional<std::string> error;

    static response_t ok(const std::string& id, nlohmann::json result);
    static response_t err(const std::string& id, const std::string& message);

    nlohmann::json to_json() const;
};
} // namespace mep
