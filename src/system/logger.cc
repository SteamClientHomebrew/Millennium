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

#include "millennium/logger.h"
#include "millennium/environment.h"

#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include <sstream>

namespace
{
struct plugin_logger_registry
{
    std::mutex mutex;
    std::vector<std::unique_ptr<plugin_logger>> loggers;
};

plugin_logger_registry& logger_registry()
{
    // Plugin on_unload messages can still arrive during bootstrap cleanup.
    static auto* registry = new plugin_logger_registry();
    return *registry;
}

std::tm local_time(std::time_t time)
{
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &time);
#else
    localtime_r(&time, &result);
#endif
    return result;
}
} // namespace

std::vector<plugin_logger*> get_plugin_logger_mgr()
{
    auto& registry = logger_registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::vector<plugin_logger*> snapshot;
    snapshot.reserve(registry.loggers.size());
    for (const auto& logger : registry.loggers) snapshot.push_back(logger.get());
    return snapshot;
}

plugin_logger& get_or_create_plugin_logger(const std::string& pluginName)
{
    auto& registry = logger_registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (const auto& logger : registry.loggers) {
        if (logger->get_plugin_name(false) == pluginName) return *logger;
    }
    registry.loggers.push_back(std::make_unique<plugin_logger>(pluginName));
    return *registry.loggers.back();
}

#ifdef _WIN32
#include "millennium/cmdline_api.h"
#include <windows.h>
#include "millennium/argp_win32.h"

void EnableVirtualTerminalProcessing()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) return;

    SetConsoleOutputCP(CP_UTF8);
}
#endif

std::string logger_base::get_local_time(bool withHours)
{
    std::stringstream bufferStream;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    const auto localTime = local_time(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    if (withHours) {
        bufferStream << std::put_time(&localTime, "%H:%M:%S");
    } else {
        bufferStream << std::put_time(&localTime, "%M:%S");
    }
    bufferStream << std::format(".{:03}", ms.count());
    return std::format("[{}]", bufferStream.str());
}

std::string logger_base::get_local_date_str()
{
    auto now = std::chrono::system_clock::now();
    std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    const auto localeTime = local_time(timeNow);

    int year = localeTime.tm_year + 1900;
    int month = localeTime.tm_mon + 1;
    int day = localeTime.tm_mday;
    int hour = localeTime.tm_hour;
    int min = localeTime.tm_min;
    int sec = localeTime.tm_sec;

    return std::format("[{}-{}-{} @ {}:{}:{}]", year, month, day, hour, min, sec);
}

plugin_logger::plugin_logger(const std::string& pluginName) : pluginName(pluginName)
{
    this->filename = (std::filesystem::path(platform::environment::get("MILLENNIUM__LOGS_PATH")) / std::format("{}_log.log", pluginName)).generic_string();
    file.open(filename, std::ios::app);

    file << std::format("\n\n\n--------------------------------- [{}] ---------------------------------\n", get_local_date_str());
    file.flush();
}

plugin_logger::~plugin_logger()
{
    file.close();
}

void plugin_logger::notify_listeners(const log_entry& entry)
{
    std::vector<std::function<void(const log_entry&)>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_listener_mutex);
        snapshot.reserve(m_listeners.size());
        for (const auto& [id, fn] : m_listeners)
            snapshot.push_back(fn);
    }
    for (const auto& fn : snapshot)
        fn(entry);
}

int plugin_logger::add_listener(std::function<void(const log_entry&)> fn)
{
    int id = ++m_listener_id_counter;
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners[id] = std::move(fn);
    return id;
}

void plugin_logger::remove_listener(int id)
{
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners.erase(id);
}

static uint64_t now_us()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void plugin_logger::log(const std::string& message, bool onlyBuffer, uint64_t timestamp_us, std::string src_file, int src_line)
{
    const log_entry entry{ message, log_level::info, timestamp_us ? timestamp_us : now_us(), std::move(src_file), src_line };
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        const std::string formatted = std::format("{} ", get_plugin_name());
        if (!onlyBuffer) {
            std::cout << std::format("{} \033[1m\033[34m{}\033[0m\033[0m", get_local_time(), formatted) << message << "\n";
            file << formatted << message << "\n";
            file.flush();
        }
        logBuffer.push_back(entry);
    }
    // Listeners may collect logs or log again; never call them with log_mutex held.
    notify_listeners(entry);
}

void plugin_logger::warn(const std::string& message, bool onlyBuffer, uint64_t timestamp_us, std::string src_file, int src_line)
{
    const log_entry entry{ message, log_level::warn, timestamp_us ? timestamp_us : now_us(), std::move(src_file), src_line };
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        const std::string formatted = std::format("{}{}{}", get_local_time(), std::format(" {} ", get_plugin_name()), message.c_str());
        if (!onlyBuffer) {
            std::cout << COL_YELLOW << formatted << COL_RESET << '\n';
            file << formatted;
            file.flush();
        }
        logBuffer.push_back(entry);
    }
    notify_listeners(entry);
}

void plugin_logger::error(const std::string& message, bool onlyBuffer, uint64_t timestamp_us, std::string src_file, int src_line)
{
    const log_entry entry{ message, log_level::error, timestamp_us ? timestamp_us : now_us(), std::move(src_file), src_line };
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        const std::string formatted = std::format("{}{}{}", get_local_time(), std::format(" {} ", get_plugin_name()), message.c_str());
        if (!onlyBuffer) {
            std::cout << COL_RED << formatted << COL_RESET << '\n';
            file << formatted;
            file.flush();
        }
        logBuffer.push_back(entry);
    }
    notify_listeners(entry);
}

void plugin_logger::print(const std::string& message)
{
    const log_entry entry{ message, log_level::info, now_us(), "", 0 };
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        file << message;
        file.flush();
        logBuffer.push_back(entry);
    }
    notify_listeners(entry);
}

std::vector<logger_base::log_entry> plugin_logger::collect_logs()
{
    std::lock_guard<std::mutex> lock(log_mutex);
    return logBuffer;
}

std::string plugin_logger::get_plugin_name(bool upperCase)
{
    const auto toUpper = [](const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    };

    return upperCase ? toUpper(pluginName) : pluginName;
}

millennium_logger& logger = millennium_logger::get_instance();

std::string millennium_logger::get_local_time_stamp()
{
    std::stringstream bufferStream;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    const auto localTime = local_time(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    bufferStream << std::put_time(&localTime, "%M:%S");
    bufferStream << std::format(".{:03}", ms.count());
    return std::format("[{}]", bufferStream.str());
}

void millennium_logger::print(std::string type, const std::string& message, std::string color)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << std::format("{}\033[1m{}{}{}\033[0m{}\n", get_local_time_stamp(), color, type, COL_RESET, message);
}

millennium_logger::millennium_logger()
{
#ifdef _WIN32
    this->m_bIsConsoleEnabled = ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState('M') & 0x8000)) || CommandLineArguments::has_argument("-dev");

    if (m_bIsConsoleEnabled) {
        (void)static_cast<bool>(AllocConsole());
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#ifdef _WIN64
        SetConsoleTitleA("Millennium_x86_64@" MILLENNIUM_VERSION);
#elif defined(_WIN32)
        SetConsoleTitleA("Millennium_i386@" MILLENNIUM_VERSION);
#endif
        EnableVirtualTerminalProcessing();
        std::ios::sync_with_stdio(true);
    }
#elif __linux__
    this->m_should_log = true;
#endif
}

void millennium_logger::log_plugin_message(std::string pluginName, std::string strMessage)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    const auto toUpper = [](const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    };

    std::string message = std::format("{} \033[1m\033[34m{} \033[0m\033[0m{}\n", get_local_time_stamp(), toUpper(pluginName), strMessage);
    std::cout << message;
}
