#pragma once
#ifdef _WIN32
#include <winbase.h>
#include <io.h>
#include <sys/log.h>
#include <fcntl.h>
#include <core/py_controller/logger.h>

auto now = std::chrono::system_clock::now();
auto currentTimeStamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

static const wchar_t* PIPE_NAME = LR"(\\.\pipe\MillenniumStdoutPipe)"; 

static const char* SHIM_LOADER_PATH        = "user32.dll";
static const char* SHIM_LOADER_QUEUED_PATH = "user32.dll.queued"; // The path of the recently updated shim loader waiting for an update.

namespace WinUtils {

    enum ShimLoaderProps {
        VALID,
        INVALID,
        FAILED
    };

    const static std::wstring GetPipeName() 
    {
        std::wstringstream pipeNameStream;
        pipeNameStream << PIPE_NAME << L"_" << currentTimeStamp;
        return pipeNameStream.str();
    }

    const static ShimLoaderProps CheckShimLoaderVersion(std::filesystem::path shimPath)
    {
        /** Load the shim loader with DONT_RESOLVE_DLL_REFERENCES to prevent DllMain from causing a boot loop/realloc on deallocated memory */
        HMODULE hModule = LoadLibraryEx(shimPath.wstring().c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);

        if (!hModule) 
        {
            LOG_ERROR("Failed to load {}: {}", shimPath.string(), GetLastError());
            return ShimLoaderProps::FAILED;
        }

        // Call __get_shim_version() from the shim module
        typedef const char* (*GetShimVersion)();
        GetShimVersion getShimVersion = (GetShimVersion)GetProcAddress(hModule, "__get_shim_version");

        if (!getShimVersion) 
        {
            LOG_ERROR("Failed to get __get_shim_version: {}", GetLastError());
            FreeLibrary(hModule);
            return ShimLoaderProps::FAILED;
        }

        std::string shimVersion = getShimVersion();
        Logger.Log("Shim version: {}", shimVersion);

        if (shimVersion != MILLENNIUM_VERSION) 
        {
            LOG_ERROR("Shim version mismatch: {} != {}", shimVersion, MILLENNIUM_VERSION);
            FreeLibrary(hModule);
            return ShimLoaderProps::INVALID;
        }

        FreeLibrary(hModule);
        return ShimLoaderProps::VALID;
    }

    const int CreateTerminalPipe(HANDLE hConsolehandle) 
    {
        const auto filename = SystemIO::GetInstallPath() / "ext" / "data" / "logs" / "stdout.log";
        std::ofstream outFile;
        outFile.open(filename, std::ios::trunc);

        HANDLE hNamedPipe = CreateNamedPipe(GetPipeName().c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 0, 0, nullptr);

        if (hNamedPipe == INVALID_HANDLE_VALUE) 
        {
            std::cout << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
            return 1;
        }
        
        if (!ConnectNamedPipe(hNamedPipe, nullptr)) 
        {
            std::cout << "Failed to connect named pipe. Error: " << GetLastError() << std::endl;
            CloseHandle(hNamedPipe);
            return 1;
        }

        char buffer[4096];
        unsigned long bytesRead;

        SetConsoleOutputCP(CP_UTF8);

        while (true) 
        {
            bool success = ReadFile(hNamedPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);

            if (!success || bytesRead == 0) 
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            buffer[bytesRead] = '\0';

            unsigned long charsWritten;
            std::string strData = std::string(buffer, bytesRead);

            std::vector<wchar_t> wideBuffer(strData.size());
            int wideLength = MultiByteToWideChar(CP_UTF8, 0, strData.c_str(), strData.size(), wideBuffer.data(), wideBuffer.size());

            if (wideLength > 0) 
            {
                DWORD charsWritten;
                WriteConsoleW(hConsolehandle, wideBuffer.data(), wideLength, &charsWritten, nullptr);
            }

            if (outFile.is_open()) 
            {
                outFile << strData;
                outFile.flush();
            }
            
            RawToLogger("Standard Output", std::string(buffer, bytesRead));
        }

        CloseHandle(hNamedPipe);
        FreeConsole();

        return 0;
    }

    const int RedirectToPipe()
    {
        if ((HANDLE)_get_osfhandle(_fileno(stdout)) == INVALID_HANDLE_VALUE)
        {
            // If for some reason the console is not allocated, stdout would be an invalid pipe handle,
            // so we first need to redirect it to a valid handle to pipe it.
            // Using NUL as a file name will redirect stdout to the void, but it's still a valid handle.
            freopen("NUL", "w", stdout);
        }

        HANDLE hPipe = CreateFileW(GetPipeName().c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

        while (hPipe == INVALID_HANDLE_VALUE)
        {
            hPipe = CreateFileW(GetPipeName().c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        int pipeDescriptor = _open_osfhandle((intptr_t)hPipe, _O_WRONLY);
        if (pipeDescriptor == -1)
        {
            CloseHandle(hPipe);
            return 1;
        }

        FILE* pipeFile = _fdopen(pipeDescriptor, "w");
        if (!pipeFile)
        {
            CloseHandle(hPipe);
            return 1;
        }

        if (_dup2(_fileno(pipeFile), _fileno(stdout)) == -1)
        {
            fclose(pipeFile);
            return 1;
        }

        setvbuf(stdout, NULL, _IONBF, 0);
        return 0;
    }

    const int RestoreStdout()
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        return 0;
    }

    __declspec(dllexport) std::unique_ptr<std::thread> terminalPipeThread;

    /**
     * @brief Redirects the standard output to a pipe to be processed by the plugin logger, and verifies the shim loader.
     */
    const void SetupWin32Environment()
    {
        // Get the terminal handle before redirecting it. 
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        if (hConsole == INVALID_HANDLE_VALUE) 
        {
            MessageBoxA(NULL, "Failed to get console handle, try restarting Steam.", "Oops!", MB_ICONERROR | MB_OK);
            return;
        }

        std::thread(WinUtils::CreateTerminalPipe, hConsole).detach();
        WinUtils::RedirectToPipe();

        // if (WinUtils::CheckShimLoaderVersion(SystemIO::GetInstallPath() / SHIM_LOADER_PATH) == ShimLoaderProps::INVALID) 
        // {
        //     MessageBoxA(NULL, "It appears one of Millennium's core assets are out of date, this may cause stability issues. It is recommended that you reinstall Millennium.", "Oops!", MB_ICONERROR | MB_OK);
        //     return;
        // }

        try 
        {
            if (!std::filesystem::exists(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH))
            {
                Logger.Log("No queued shim loader found...");
                return;
            }

            if (CheckShimLoaderVersion(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH) == ShimLoaderProps::INVALID)
            {
                MessageBoxA(NULL, "There is a version mismatch between two of Millenniums core assets 'user32.queue.dll' and 'millennium.dll' in the Steam directory. Try removing 'user32.queue.dll', and if that doesn't work reinstall Millennium.", "Oops!", MB_ICONERROR | MB_OK);
                return;
            }

            Logger.Log("Updating shim module from cache...");

            const int TIMEOUT_IN_SECONDS = 10;
            auto startTime = std::chrono::steady_clock::now();

            // Wait for the preloader to be removed, as sometimes it's still in use for a few milliseconds.
            while (true) 
            {
                try 
                {
                    std::filesystem::remove(SystemIO::GetInstallPath() / SHIM_LOADER_PATH);
                    break;
                }
                catch (std::filesystem::filesystem_error& e)
                {
                    if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(TIMEOUT_IN_SECONDS))
                    {
                        throw std::runtime_error("Timed out while waiting for the preloader to be removed.");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }

            Logger.Log("Removed old inject shim...");

            std::filesystem::rename(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH, SystemIO::GetInstallPath() / SHIM_LOADER_PATH); 
            Logger.Log("Successfully updated {}!", SHIM_LOADER_PATH);
        }
        catch (std::exception& e) 
        {
            LOG_ERROR("Failed to update {}: {}", SHIM_LOADER_PATH, e.what());
            MessageBoxA(NULL, fmt::format("Failed to update {}, it's recommended that you reinstall Millennium.", SHIM_LOADER_PATH).c_str(), "Oops!", MB_ICONERROR | MB_OK);
        }
    }
}
#endif
