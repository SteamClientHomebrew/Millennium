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

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <condition_variable>

#ifdef _WIN32
#include "millennium/argp_win32.h"
#endif
#include "millennium/env.h"
#include "millennium/logger.h"
#include "millennium/stdout_tee.h"
#include <unistd.h>

OutputLogger Logger;

std::string OutputLogger::GetLocalTime()
{
    std::stringstream bufferStream;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    bufferStream << std::put_time(std::localtime(&time), "%M:%S");
    bufferStream << fmt::format(".{:03}", ms.count());
    return fmt::format("[{}]", bufferStream.str());
}

void OutputLogger::PrintMessage(std::string type, const std::string& message, std::string color)
{
    std::lock_guard<std::mutex> lock(logMutex);
    millennium::cout << fmt::format("{}\033[1m{}{}{}\033[0m{}\n", GetLocalTime(), color, type, COL_RESET, message);
}

#ifdef _WIN32
void EnableVirtualTerminalProcessing(HANDLE hRealConsole)
{
    /** enable VT output (for ansi) */
    if (hRealConsole != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hRealConsole, &dwMode)) {
            /** enable VT processing and keep processed output flag */
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            (void)SetConsoleMode(hRealConsole, dwMode);
        }
    }

    /** enable VT input so cursor/arrow keys and other VT input sequences are delivered */
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD inMode = 0;
        if (GetConsoleMode(hIn, &inMode)) {
            inMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            (void)SetConsoleMode(hIn, inMode);
        }
    }

    SetConsoleOutputCP(CP_UTF8);
}
#endif

extern std::mutex hasShownDevToolsMutex;
extern std::condition_variable hasShownDevToolsCV;

#ifdef _WIN32
HANDLE g_millenniumConsoleHandle;

/**
 * redirect std::cout to the console window from the handle directly
 * this avoids issues with freopen and flushing
 */
class ConsoleStreamBuf : public std::streambuf
{
    HANDLE hConsole;

  protected:
    virtual int overflow(int c) override
    {
        if (c != EOF) {
            char ch = static_cast<char>(c);
            DWORD written;
            WriteConsoleA(hConsole, &ch, 1, &written, NULL);
        }
        return c;
    }
    virtual std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        DWORD written;
        WriteConsoleA(hConsole, s, static_cast<DWORD>(n), &written, NULL);
        return written;
    }

  public:
    ConsoleStreamBuf(HANDLE h) : hConsole(h)
    {
    }
};
#endif

OutputLogger::OutputLogger()
{
#ifdef _WIN32
    this->m_bIsConsoleEnabled = ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState('M') & 0x8000)) || CommandLineArguments::HasArgument("-dev");
    if (m_bIsConsoleEnabled) {
        (void)static_cast<bool>(AllocConsole());
        // freopen("CONOUT$", "w", stdout);
        // freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);

        g_millenniumConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

        freopen("NUL", "w", stdout);
        freopen("NUL", "w", stderr);

        SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
        SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);

        static ConsoleStreamBuf consoleBuffer(g_millenniumConsoleHandle);
        std::cout.rdbuf(&consoleBuffer);

        EnableVirtualTerminalProcessing(g_millenniumConsoleHandle);
        std::ios::sync_with_stdio(true);
    }
#elif __linux__
    this->m_bIsConsoleEnabled = true;
#endif
}

void OutputLogger::LogPluginMessage(std::string pluginName, std::string strMessage)
{
    std::lock_guard<std::mutex> lock(logMutex);

    const auto toUpper = [](const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    };

    std::string message = fmt::format("{} \033[1m\033[34m{} \033[0m\033[0m{}\n", GetLocalTime(), toUpper(pluginName), strMessage);
    millennium::cout << message;
}
