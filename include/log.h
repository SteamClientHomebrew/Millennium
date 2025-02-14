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
#include <fstream>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <mutex>
#include <fmt/core.h>
#include <fmt/color.h>
#include <memory>
#include <iostream>

#define DEFAULT_ACCENT_COL fg(fmt::color::light_sky_blue)

#define COL_RED "\033[31m"
#define COL_GREEN "\033[32m"
#define COL_YELLOW "\033[33m"
#define COL_BLUE "\033[34m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN "\033[36m"
#define COL_WHITE "\033[37m"
#define COL_RESET "\033[0m"

class OutputLogger
{
private:
#ifdef _WIN32
    bool m_bIsVersbose = false;
#elif __linux__
    bool m_bIsVersbose = true;
#endif
    bool m_bIsConsoleEnabled = false;
    std::mutex logMutex;
    std::shared_ptr<std::ostream> teeStreamPtr;
    std::shared_ptr<std::ofstream> outputLogStream;
    std::string GetLocalTime();

public:
    void PrintMessage(std::string type, const std::string &message, std::string color = COL_WHITE);

    OutputLogger(const OutputLogger &) = delete;
    OutputLogger &operator=(const OutputLogger &) = delete;

    OutputLogger();
    ~OutputLogger();

    void LogPluginMessage(std::string pname, std::string val);

    template <typename... Args>
    void Log(std::string fmt, Args &&...args)
    {
        PrintMessage(" INFO ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_GREEN);
    }

    template <typename... Args>
    void ErrorTrace(std::string fmt, const char* file, int line, const char* function, Args &&...args)
    {
        this->PrintMessage(" ERROR ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_RED);
        this->PrintMessage(" TRACE ", fmt::format("{} @ {}:{}", function, file, line), COL_RED);
    }

    template <typename... Args>
    void Warn(std::string fmt, Args &&...args)
    {
        PrintMessage(" WARN ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_YELLOW);
    }

    void LogHead(std::string val, fmt::text_style color = fg(fmt::color::magenta));
    void LogItem(std::string pluginName, std::string data, bool end = false, fmt::text_style color = fg(fmt::color::magenta));
};

extern OutputLogger Logger;

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) Logger.ErrorTrace(fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif