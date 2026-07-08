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

#include "mep/log_normalize.h"

#include <algorithm>
#include <cstring>

using json = nlohmann::json;

namespace mep
{
namespace
{
const char* level_str(logger_base::log_level lv)
{
    switch (lv) {
        case logger_base::log_level::warn:
            return "warn";
        case logger_base::log_level::error:
            return "error";
        default:
            return "info";
    }
}
} // namespace

json normalize_log_entry(const logger_base::log_entry& e)
{
    json obj = {
        { "source",       "backend"          },
        { "level",        level_str(e.level) },
        { "message",      e.message          },
        { "timestamp_us", e.timestamp_us     },
    };
    if (!e.file.empty()) {
        obj["file"] = e.file;
        obj["line"] = e.line;
    }
    return obj;
}

json normalize_console_entry(const console_entry& e)
{
    const json& r = e.raw;
    uint64_t ts = r.contains("millennium_ts") ? r["millennium_ts"].get<uint64_t>() : uint64_t(0);

    std::string level = "log";
    if (r.contains("level") && r["level"].is_string())
        level = r["level"].get<std::string>();
    else if (r.contains("type") && r["type"].is_string())
        level = r["type"].get<std::string>();

    std::string message;
    if (r.contains("message") && r["message"].is_string()) message = r["message"].get<std::string>();

    std::string src_file;
    int src_line = 0;
    if (r.contains("file") && r["file"].is_string()) {
        src_file = r["file"].get<std::string>();
        if (r.contains("line") && r["line"].is_number()) src_line = r["line"].get<int>();
    } else if (r.contains("stackTrace") && r["stackTrace"].contains("callFrames")) {
        for (const auto& frame : r["stackTrace"]["callFrames"]) {
            if (!frame.contains("url") || !frame["url"].is_string()) continue;
            const std::string url = frame["url"].get<std::string>();
            if (url.empty()) continue;
            if (url.rfind("chrome://", 0) == 0 || url.rfind("chrome-extension://", 0) == 0) continue;

            std::string path = url;
            for (const auto& prefix : { "file://", "https://", "http://" }) {
                if (path.rfind(prefix, 0) == 0) {
                    path = path.substr(strlen(prefix));
                    break;
                }
            }
            const size_t slash = path.rfind('/');
            src_file = (slash != std::string::npos) ? path.substr(slash + 1) : path;
            if (frame.contains("lineNumber") && frame["lineNumber"].is_number()) src_line = frame["lineNumber"].get<int>() + 1;
            break;
        }
    }

    std::string source = "frontend";
    if (r.contains("millennium_source") && r["millennium_source"].is_string()) source = r["millennium_source"].get<std::string>();

    json obj = {
        { "source",       source  },
        { "level",        level   },
        { "message",      message },
        { "timestamp_us", ts      },
    };
    if (!src_file.empty()) {
        obj["file"] = src_file;
        obj["line"] = src_line;
    }
    return obj;
}

json normalize_patch_entry(const patch_event& e)
{
    std::string message;
    if (e.event_type == "conflict") {
        message = std::format("patch conflict with '{}' on {}", e.other_plugin, e.filename);
    } else if (e.event_type == "syntax_error") {
        message = std::format("regex syntax error in {}", e.filename);
    } else if (e.event_type == "url_seen") {
        message = std::format("resource seen: {}", e.filename);
    } else {
        message = std::format("{} on {}", e.event_type, e.filename);
    }
    if (!e.detail.empty()) message += ": " + e.detail;

    return {
        { "source",             "hhx64"                  },
        { "event_type",         e.event_type             },
        { "plugin",             e.plugin_name            },
        { "filename",           e.filename               },
        { "message",            message                  },
        { "detail",             e.detail                 },
        { "other_plugin",       e.other_plugin           },
        { "find_pattern",       e.find_pattern           },
        { "transform_patterns", e.transform_patterns     },
        { "before_text",        e.before_text            },
        { "after_text",         e.after_text             },
        { "timestamp_ms",       e.timestamp_ms           },
        { "timestamp_us",       e.timestamp_ms * 1000ULL },
    };
}

json collect_log_snapshot(plugin_logger& backend_logger, std::size_t max_per_source)
{
    const std::string plugin_name = backend_logger.get_plugin_name(false);

    std::vector<json> entries;

    for (const auto& e : backend_logger.collect_logs()) {
        entries.push_back(normalize_log_entry(e));
    }
    for (const auto& e : console_capture::instance().get_recent(plugin_name, max_per_source)) {
        entries.push_back(normalize_console_entry(e));
    }
    for (const auto& e : patch_stream_recorder::instance().get_recent(plugin_name, max_per_source)) {
        entries.push_back(normalize_patch_entry(e));
    }

    std::sort(entries.begin(), entries.end(), [](const json& a, const json& b)
    {
        return a.value("timestamp_us", uint64_t(0)) < b.value("timestamp_us", uint64_t(0));
    });

    json snapshot = json::array();
    for (auto& e : entries) {
        snapshot.push_back(std::move(e));
    }
    return snapshot;
}
} // namespace mep
