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

#include "millennium/singleton.h"

#include <array>
#include <atomic>
#include <ctime>
#include <format>
#include <functional>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[38;5;214m"
#define BLUE "\x1b[94m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define RESET "\033[0m"

#define COL_RED "\033[31m"
#define COL_GREEN "\033[32m"
#define COL_YELLOW "\033[33m"
#define COL_BLUE "\033[34m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN "\033[36m"
#define COL_WHITE "\033[37m"
#define COL_RESET "\033[0m"

#define MILLENNIUM_REPOSITORY "https://github.com/SteamClientHomebrew/Millennium"

constexpr std::array<char, 1024> __sanitize_nt(const char* path)
{
    std::array<char, 1024> result{};
    size_t i = 0;
    while (path[i] != '\0') {
        result[i] = (path[i] == '\\') ? '/' : path[i];
        ++i;
    }
    result[i] = '\0';
    return result;
}

constexpr std::string_view __get_source_loc(const char* file)
{
    std::string_view srcPath(file);
#ifdef MILLENNIUM_ROOT
    std::string_view root(MILLENNIUM_ROOT);
    return srcPath.size() >= root.size() && srcPath.compare(0, root.size(), root) == 0 ? srcPath.substr(root.length()) : srcPath;
#else
    return srcPath;
#endif
}

class logger_base
{
  public:
    enum class log_level
    {
        info,
        warn,
        error
    };

    struct log_entry
    {
        std::string message;
        log_level level;
        uint64_t timestamp_us;
        std::string file;
        int line{ 0 };
    };

  protected:
    std::mutex log_mutex;
    std::vector<log_entry> logBuffer;

    std::string get_local_time(bool withHours = false);
    std::string get_local_date_str();

  public:
    virtual ~logger_base() = default;
};

class plugin_logger : public logger_base
{
  private:
    std::ofstream file;
    std::string filename;
    std::string pluginName;

    std::mutex m_listener_mutex;
    std::unordered_map<int, std::function<void(const log_entry&)>> m_listeners;
    std::atomic<int> m_listener_id_counter{ 0 };

    void notify_listeners(const log_entry& entry);

  public:
    plugin_logger(const std::string& pluginName);
    ~plugin_logger();

    void log(const std::string& message, bool onlyBuffer = false, uint64_t timestamp_us = 0, std::string file = "", int line = 0);
    void warn(const std::string& message, bool onlyBuffer = false, uint64_t timestamp_us = 0, std::string file = "", int line = 0);
    void error(const std::string& message, bool onlyBuffer = false, uint64_t timestamp_us = 0, std::string file = "", int line = 0);
    void print(const std::string& message);

    std::vector<log_entry> collect_logs();
    std::string get_plugin_name(bool upperCase = true);

    /**
     * Register a listener that is called with each new log entry as it is written.
     * Safe to call from any thread. Returns a listener ID for use with remove_listener().
     */
    int add_listener(std::function<void(const log_entry&)> fn);

    /**
     * Deregister the listener with the given ID.
     * Safe to call from any thread, including during an active notify_listeners() call.
     * A no-op if @p id is unknown or already removed.
     */
    void remove_listener(int id);
};

class millennium_logger : public logger_base, public singleton<millennium_logger>
{
    friend class singleton<millennium_logger>;

  private:
    bool m_should_log = false;
    [[maybe_unused]] bool m_bIsConsoleEnabled = false;

    std::string get_local_time_stamp();
    millennium_logger();

  public:
    void print(std::string type, const std::string& message, std::string color = COL_WHITE);
    void log_plugin_message(std::string pname, std::string val);

    template <typename... Args> void log(std::string fmt_str, Args&&... args)
    {
        print(" INFO ", (sizeof...(args) == 0) ? fmt_str : std::vformat(fmt_str, std::make_format_args(args...)), COL_GREEN);
    }

    template <typename... Args> void private_error_do_not_use(std::string fmt_str, const char* file, int line, const char* function, Args&&... args)
    {
        std::string remoteRepository = std::format("{}/blob/{}{}#L{}", MILLENNIUM_REPOSITORY, GIT_COMMIT_HASH, __get_source_loc(file).data(), line);

        print(" ERROR ", (sizeof...(args) == 0) ? fmt_str : std::vformat(fmt_str, std::make_format_args(args...)), COL_RED);
        print(" * FUNCTION: ", function, COL_RED);
        print(" * LOCATION: ", remoteRepository, COL_RED);
    }

    template <typename... Args> void warn(std::string fmt_str, Args&&... args)
    {
        print(" WARN ", (sizeof...(args) == 0) ? fmt_str : std::vformat(fmt_str, std::make_format_args(args...)), COL_YELLOW);
    }
};

extern millennium_logger& logger;

inline std::string format_log_timestamp(uint64_t timestamp_us)
{
    const time_t sec = static_cast<time_t>(timestamp_us / 1'000'000);
    struct tm t{};
#ifdef _WIN32
    localtime_s(&t, &sec);
#else
    localtime_r(&sec, &t);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &t);
    return buf;
}

inline std::vector<plugin_logger*>& get_plugin_logger_mgr()
{
    static std::vector<plugin_logger*> loggerList;
    return loggerList;
}

static void add_logger_message(const std::string pluginName, const std::string message, logger_base::log_level level, const bool v2, uint64_t timestamp_us = 0,
                               std::string file = "", int line = 0)
{
    const auto add_log = [v2, timestamp_us, &file, line](plugin_logger* logger, const std::string& message, const logger_base::log_level& level)
    {
        switch (level) {
            case logger_base::log_level::info:
                logger->log(message, v2, timestamp_us, file, line);
                break;
            case logger_base::log_level::warn:
                logger->warn(message, v2, timestamp_us, file, line);
                break;
            case logger_base::log_level::error:
                logger->error(message, v2, timestamp_us, file, line);
                break;
        }
    };

    for (auto logger : get_plugin_logger_mgr()) {
        if (logger->get_plugin_name(false) == pluginName) {
            add_log(logger, message, level);
            return;
        }
    }

    plugin_logger* newLogger = new plugin_logger(pluginName);
    add_log(newLogger, message, level);
    get_plugin_logger_mgr().push_back(newLogger);
}

inline void InfoToLogger(const std::string pluginNme, const std::string message, const bool v2, uint64_t timestamp_us = 0, std::string file = "", int line = 0)
{
    add_logger_message(pluginNme, message, logger_base::log_level::info, v2, timestamp_us, std::move(file), line);
}

inline void WarnToLogger(const std::string pluginNme, const std::string message, const bool v2, uint64_t timestamp_us = 0, std::string file = "", int line = 0)
{
    add_logger_message(pluginNme, message, logger_base::log_level::warn, v2, timestamp_us, std::move(file), line);
}

inline void ErrorToLogger(const std::string pluginNme, const std::string message, const bool v2, uint64_t timestamp_us = 0, std::string file = "", int line = 0)
{
    add_logger_message(pluginNme, message, logger_base::log_level::error, v2, timestamp_us, std::move(file), line);
}

inline void RawToLogger(const std::string pluginName, const std::string message)
{
    for (auto logger : get_plugin_logger_mgr()) {
        if (logger->get_plugin_name(false) == pluginName) {
            logger->print(message);
            return;
        }
    }

    plugin_logger* logger = new plugin_logger(pluginName);
    logger->print(message);
    get_plugin_logger_mgr().push_back(logger);
}

void CleanupLoggers();

#if defined(_MSC_VER)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif
#ifndef LOG_ERROR
#if defined(__clang__)
#define LOG_ERROR(...) LOG_ERROR_IMPL(NUM(__VA_ARGS__), __VA_ARGS__)
#define LOG_ERROR_IMPL(qty, ...) LOG_ERROR_IMPL2(qty, __VA_ARGS__)
#define LOG_ERROR_IMPL2(qty, ...) LOG_ERROR_##qty(__VA_ARGS__)
#define LOG_ERROR_ONE(fmt) logger.private_error_do_not_use(fmt, __sanitize_nt(__FILE__).data(), __LINE__, PRETTY_FUNCTION)
#define LOG_ERROR_TWOORMORE(fmt, ...) logger.private_error_do_not_use(fmt, __sanitize_nt(__FILE__).data(), __LINE__, PRETTY_FUNCTION, __VA_ARGS__)
#define NUM(...) SELECT_11TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_11TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#else
#define LOG_ERROR(fmt, ...) logger.private_error_do_not_use(fmt, __sanitize_nt(__FILE__).data(), __LINE__, PRETTY_FUNCTION, ##__VA_ARGS__)
#endif
#endif
#define GET_GITHUB_URL_FROM_HERE() std::format("{}/blob/{}{}#L{}", MILLENNIUM_REPOSITORY, GIT_COMMIT_HASH, __get_source_loc(__sanitize_nt(__FILE__).data()).data(), __LINE__)
