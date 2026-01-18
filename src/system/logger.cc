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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "millennium/logger.h"
#include <algorithm>
#include <fcntl.h>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
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
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    if (withHours) {
        bufferStream << std::put_time(std::localtime(&time), "%H:%M:%S");
    } else {
        bufferStream << std::put_time(std::localtime(&time), "%M:%S");
    }
    bufferStream << fmt::format(".{:03}", ms.count());
    return fmt::format("[{}]", bufferStream.str());
}

std::string logger_base::get_local_date_str()
{
    auto now = std::chrono::system_clock::now();
    std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    std::tm localeTime = *std::localtime(&timeNow);

    int year = localeTime.tm_year + 1900;
    int month = localeTime.tm_mon + 1;
    int day = localeTime.tm_mday;
    int hour = localeTime.tm_hour;
    int min = localeTime.tm_min;
    int sec = localeTime.tm_sec;

    return fmt::format("[{}-{}-{} @ {}:{}:{}]", year, month, day, hour, min, sec);
}

plugin_logger::plugin_logger(const std::string& pluginName) : pluginName(pluginName)
{
    this->filename = (std::filesystem::path(GetEnv("MILLENNIUM__LOGS_PATH")) / fmt::format("{}_log.log", pluginName)).generic_string();
    file.open(filename, std::ios::app);

    file << fmt::format("\n\n\n--------------------------------- [{}] ---------------------------------\n", get_local_date_str());
    file.flush();
}

plugin_logger::~plugin_logger()
{
    file.close();
}

void plugin_logger::log(const std::string& message, bool onlyBuffer)
{
    std::string formatted = fmt::format("{} ", get_plugin_name());

    if (!onlyBuffer) {
        std::cout << fmt::format("{} \033[1m\033[34m{}\033[0m\033[0m", get_local_time(), formatted) << message << "\n";

        file << formatted << message << "\n";
        file.flush();
    }

    logBuffer.push_back({ WHITE + get_local_time(true) + RESET + " " + BLUE + formatted + RESET + message + "\n", log_level::INFO });
}

void plugin_logger::warn(const std::string& message, bool onlyBuffer)
{
    if (!onlyBuffer) {
        std::string formatted = fmt::format("{}{}{}", get_local_time(), fmt::format(" {} ", get_plugin_name()), message.c_str());
        std::cout << COL_YELLOW << formatted << COL_RESET << '\n';

        file << formatted;
        file.flush();
    }

    logBuffer.push_back({ WHITE + get_local_time(true) + YELLOW + " " + get_plugin_name() + " " + message + RESET + "\n", log_level::WARN });
}

void plugin_logger::error(const std::string& message, bool onlyBuffer)
{
    if (!onlyBuffer) {
        std::string formatted = fmt::format("{}{}{}", get_local_time(), fmt::format(" {} ", get_plugin_name()), message.c_str());
        std::cout << COL_RED << formatted << COL_RESET << '\n';

        file << formatted;
        file.flush();
    }

    logBuffer.push_back({ RED + message + RESET, log_level::ERROR });
}

void plugin_logger::print(const std::string& message)
{
    file << message;
    file.flush();

    logBuffer.push_back({ message, log_level::INFO });
}

std::vector<logger_base::log_entry> plugin_logger::collect_logs()
{
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
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    bufferStream << std::put_time(std::localtime(&time), "%M:%S");
    bufferStream << fmt::format(".{:03}", ms.count());
    return fmt::format("[{}]", bufferStream.str());
}

void millennium_logger::print(std::string type, const std::string& message, std::string color)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << fmt::format("{}\033[1m{}{}{}\033[0m{}\n", get_local_time_stamp(), color, type, COL_RESET, message);
}

millennium_logger::millennium_logger()
{
#ifdef _WIN32
    this->m_bIsConsoleEnabled = ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState('M') & 0x8000)) || CommandLineArguments::HasArgument("-dev");

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

    std::string message = fmt::format("{} \033[1m\033[34m{} \033[0m\033[0m{}\n", get_local_time_stamp(), toUpper(pluginName), strMessage);
    std::cout << message;
}
