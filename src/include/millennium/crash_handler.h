
#include <exception>

#ifdef _WIN32
#include "millennium/http_hooks.h"
#include "millennium/steam_hooks_win32.h"
#include <DbgHelp.h>
#include <cxxabi.h>
#include <dbghelp.h>
#include <fmt/core.h>
#include <memory>
#include <shellapi.h>
#include <string>
#include <windows.h>

std::string DemangleSymbolName(const char* mangledName)
{
    int status = 0;
    std::unique_ptr<char, void (*)(void*)> symbolDemangler(abi::__cxa_demangle(mangledName, nullptr, nullptr, &status), std::free);
    return (status == 0 && symbolDemangler) ? std::string(symbolDemangler.get()) : std::string(mangledName);
}

bool InitializeSymbolHandler(HANDLE process, std::string& errorMessage)
{
    DWORD options = SymGetOptions();
    options |= SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS;
    options &= ~SYMOPT_UNDNAME;
    SymSetOptions(options);

    if (!SymInitialize(process, nullptr, TRUE)) {
        DWORD errorCode = GetLastError();
        errorMessage.append(fmt::format("\nFailed to initialize symbol handler. Error: {}\n", errorCode));
        return false;
    }

    return true;
}

std::string FormatStackFrame(HANDLE process, void* address, USHORT index, SYMBOL_INFO* symbol, IMAGEHLP_LINE64& lineInfo)
{
    DWORD64 addr = reinterpret_cast<DWORD64>(address);
    DWORD64 displacement = 0;

    if (!SymFromAddr(process, addr, &displacement, symbol)) {
        return fmt::format("#{}: 0x{:X} (unknown symbol, error: {})\n", index, addr, GetLastError());
    }

    /** MinGW symbols often have mangled names starting with _Z or _N */
    std::string functionName = symbol->Name[0] == '_' && (symbol->Name[1] == 'Z' || symbol->Name[1] == 'N') ? DemangleSymbolName(symbol->Name) : symbol->Name;

    DWORD lineDisplacement = 0;
    if (SymGetLineFromAddr64(process, addr, &lineDisplacement, &lineInfo)) {
        return fmt::format("#{}: {} in {} at line {} +{}\n", index, functionName, lineInfo.FileName, lineInfo.LineNumber, lineDisplacement);
    } else {
        return fmt::format("#{}: {} at 0x{:X} (no line info, error: {})\n", index, functionName, addr, GetLastError());
    }
}

void CaptureStackTrace(std::string& errorMessage, const int maxFrames = 256)
{
    HANDLE process = GetCurrentProcess();

    if (!InitializeSymbolHandler(process, errorMessage))
        return;

    std::unique_ptr<void*[]> stack(new void*[maxFrames]);
    const USHORT frameCount = CaptureStackBackTrace(0, maxFrames, stack.get(), nullptr);

    constexpr int MAX_SYMBOL_NAME_LENGTH = 1024;
    SYMBOL_INFO* symbol = static_cast<SYMBOL_INFO*>(calloc(1, sizeof(SYMBOL_INFO) + MAX_SYMBOL_NAME_LENGTH * sizeof(char)));

    if (!symbol) {
        errorMessage.append("\nFailed to allocate memory for symbol information.\n");
        SymCleanup(process);
        return;
    }

    symbol->MaxNameLen = MAX_SYMBOL_NAME_LENGTH - 1;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 lineInfo = {};
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    errorMessage.append(fmt::format("\nStack trace ({} frames):\n", frameCount));

    for (USHORT i = 0; i < frameCount; ++i) {
        errorMessage.append(FormatStackFrame(process, stack[i], i, symbol, lineInfo));
    }

    free(symbol);
    SymCleanup(process);
}

#endif

/**
 * @brief Custom terminate handler for Millennium.
 * This function is called when Millennium encounters a fatal error that it can't recover from.
 */
void UnhandledExceptionHandler()
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
    // Capture and print stack trace
    CaptureStackTrace(errorMessage);
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