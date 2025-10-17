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

#include <exception>
#include <cxxabi.h>
#include <fmt/format.h>

#ifdef _WIN32
#include "millennium/http_hooks.h"
#include "millennium/steam_hooks_win32.h"

#include <dbghelp.h>
#include <shellapi.h>
#include <string>

/**
 * @brief Custom crash handler for Millennium on Windows.
 * This function is called when an unhandled exception occurs.
 * It creates a minidump file named "millennium-crash.dmp" in the current working directory.
 *
 * @param pExceptionInfo Pointer to the exception information.
 * @return EXCEPTION_EXECUTE_HANDLER to indicate that the exception has been handled.
 */
static long __attribute__((__stdcall__)) Win32_CrashHandler(EXCEPTION_POINTERS* pExceptionInfo)
{
    HANDLE hFile = CreateFileA("millennium-crash.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ExceptionPointers = pExceptionInfo;
        dumpInfo.ClientPointers = FALSE;

        /** Write the dump */
        BOOL success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemory, &dumpInfo, nullptr, nullptr);

        if (success) {
            MessageBoxA(
                NULL,
                "Millennium has crashed! A crash dump has been written to millennium-crash.dmp in the Steam directory."
                "Please send this file to the developers on our Discord (steambrew.app/discord) or GitHub (github.com/SteamClientHomebrew/Millennium) to help fix this issue.",
                "Millennium Crash", MB_ICONERROR | MB_OK);

            Logger.Log("Crash dump written to millennium-crash.dmp");
        } else {
            Logger.Log("MiniDumpWriteDump failed. Error: {}", GetLastError());
        }

        CloseHandle(hFile);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif

/**
 * @brief Custom terminate handler for Millennium.
 * This function is called when Millennium encounters a fatal error that it can't recover from.
 */
static void UnhandledExceptionHandler()
{
#ifdef _WIN32
    if (IsDebuggerPresent())
        __debugbreak();
#endif

    auto const exceptionPtr = std::current_exception();
    std::string errorMessage = "Millennium has a fatal error that it can't recover from, check the logs for more details!\n";

    if (exceptionPtr) {
        try {
            int status;
            errorMessage.append(fmt::format("Terminating with uncaught exception of type `{}`", abi::__cxa_demangle(abi::__cxa_current_exception_type()->name(), 0, 0, &status)));
            std::rethrow_exception(exceptionPtr);
        } catch (const std::exception& e) {
            errorMessage.append(fmt::format(" with `what()` = \"{}\"", e.what()));
        } catch (...) {
        }
    }

#ifdef _WIN32
    MessageBoxA(NULL, errorMessage.c_str(), "Oops!", MB_ICONERROR | MB_OK);

    auto result = MessageBoxA(
        NULL,
        "Would you like to open your logs folder?\nLook for a file called \"Standard Output_log.log\" "
        "in the logs folder, that will have important debug information.\n\n"
        "Please send this file to Millennium developers on our discord (steambrew.app/discord), it will help prevent this error from happening to you or others in the future.",
        "Error Recovery", MB_ICONEXCLAMATION | MB_YESNO);

    if (result == IDYES) {
        std::string logPath = (SystemIO::GetInstallPath() / "ext" / "logs").string();
        ShellExecuteA(NULL, "open", logPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

#elif __linux__
    std::cerr << errorMessage << std::endl;
#endif
}
