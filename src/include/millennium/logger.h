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

#include "millennium/env.h"
#include "millennium/singleton.h"

#include <Python.h>
#include <array>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>
#include <mutex>
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

#define DEFAULT_ACCENT_COL fg(fmt::color::light_sky_blue)

constexpr std::array<char, 1024> constexpr_sanitize_nt_path_spec(const char* path)
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

constexpr std::string_view constexpr_get_relative_path(const char* file)
{
    std::string_view srcPath(file);
    std::string_view root(MILLENNIUM_ROOT);

    return srcPath.size() >= root.size() && srcPath.compare(0, root.size(), root) == 0 ? srcPath.substr(root.length()) : srcPath;
}

class logger_base
{
  public:
    enum LogLevel
    {
        _INFO,
        _WARN,
        _ERROR
    };

    struct LogEntry
    {
        std::string message;
        LogLevel level;
    };

  protected:
    std::mutex log_mutex;
    std::vector<LogEntry> logBuffer;

    std::string GetLocalTime(bool withHours = false);
    std::string GetLocalDateStr();

  public:
    virtual ~logger_base() = default;
};

class plugin_logger : public logger_base
{
  private:
    std::ofstream file;
    std::string filename;
    std::string pluginName;

  public:
    plugin_logger(const std::string& pluginName);
    ~plugin_logger();

    void Log(const std::string& message, bool onlyBuffer = false);
    void Warn(const std::string& message, bool onlyBuffer = false);
    void Error(const std::string& message, bool onlyBuffer = false);
    void Print(const std::string& message);

    std::vector<LogEntry> CollectLogs();
    std::string GetPluginName(bool upperCase = true);
};

class logger : public logger_base, public Singleton<logger>
{
    friend class Singleton<logger>;

  private:
    bool m_should_log = false;
    bool m_bIsConsoleEnabled = false;

    std::string get_local_time_stamp();
    logger();

  public:
    void print(std::string type, const std::string& message, std::string color = COL_WHITE);
    void log_plugin_message(std::string pname, std::string val);

    template <typename... Args> void Log(std::string fmt, Args&&... args)
    {
        print(" INFO ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_GREEN);
    }

    template <typename... Args> void ErrorTrace(std::string fmt, const char* file, int line, const char* function, Args&&... args)
    {
        std::string remoteRepository =
            fmt::format("https://github.com/SteamClientHomebrew/Millennium/blob/{}{}#L{}", GIT_COMMIT_HASH, constexpr_get_relative_path(file).data(), line);

        print(" ERROR ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_RED);
        print(" * FUNCTION: ", function, COL_RED);
        print(" * LOCATION: ", remoteRepository, COL_RED);
    }

    template <typename... Args> void Warn(std::string fmt, Args&&... args)
    {
        print(" WARN ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_YELLOW);
    }
};

extern logger& Logger;

inline std::vector<plugin_logger*>& get_plugin_logger_mgr()
{
    static std::vector<plugin_logger*> loggerList;
    return loggerList;
}

static void AddLoggerMessage(const std::string pluginName, const std::string message, logger_base::LogLevel level)
{
    for (auto logger : get_plugin_logger_mgr()) {
        if (logger->GetPluginName(false) == pluginName) {
            switch (level) {
                case logger_base::_INFO:
                    logger->Log(message, true);
                    break;
                case logger_base::_WARN:
                    logger->Warn(message, true);
                    break;
                case logger_base::_ERROR:
                    logger->Error(message, true);
                    break;
            }
            return;
        }
    }

    plugin_logger* newLogger = new plugin_logger(pluginName);

    switch (level) {
        case logger_base::_INFO:
            newLogger->Log(message, true);
            break;
        case logger_base::_WARN:
            newLogger->Warn(message, true);
            break;
        case logger_base::_ERROR:
            newLogger->Error(message, true);
            break;
    }

    get_plugin_logger_mgr().push_back(newLogger);
}

inline void InfoToLogger(const std::string pluginNme, const std::string message)
{
    AddLoggerMessage(pluginNme, message, logger_base::_INFO);
}

inline void WarnToLogger(const std::string pluginNme, const std::string message)
{
    AddLoggerMessage(pluginNme, message, logger_base::_WARN);
}

inline void ErrorToLogger(const std::string pluginNme, const std::string message)
{
    AddLoggerMessage(pluginNme, message, logger_base::_ERROR);
}

inline void RawToLogger(const std::string pluginName, const std::string message)
{
    for (auto logger : get_plugin_logger_mgr()) {
        if (logger->GetPluginName(false) == pluginName) {
            logger->Print(message);
            return;
        }
    }

    plugin_logger* newLogger = new plugin_logger(pluginName);
    newLogger->Print(message);
    get_plugin_logger_mgr().push_back(newLogger);
}

void CleanupLoggers();

typedef struct
{
    PyObject_HEAD char* prefix;
    plugin_logger* m_loggerPtr;
} LoggerObject;

extern PyTypeObject LoggerType;
PyObject* PyInit_Logger(void);

#if defined(_MSC_VER)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#if defined(__clang__)
#ifndef LOG_ERROR
#define LOG_ERROR(...) Logger.ErrorTrace(FIRST(__VA_ARGS__), __FILE__, __LINE__, PRETTY_FUNCTION REST(__VA_ARGS__))
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) SELECT_10TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_10TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10
#endif
#else
#define LOG_ERROR(fmt, ...) Logger.ErrorTrace(fmt, constexpr_sanitize_nt_path_spec(__FILE__).data(), __LINE__, PRETTY_FUNCTION, ##__VA_ARGS__)
#endif

#define GET_GITHUB_URL_FROM_HERE()                                                                                                                                                 \
    fmt::format("https://github.com/SteamClientHomebrew/Millennium/blob/{}{}#L{}", GIT_COMMIT_HASH,                                                                                \
                constexpr_get_relative_path(constexpr_sanitize_nt_path_spec(__FILE__).data()).data(), __LINE__)
