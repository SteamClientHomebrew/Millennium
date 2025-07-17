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
/** Force include of winsock before windows */
#include <winsock2.h>
#include <windows.h>
#endif

#include <windows.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <shimlogger.h>
#include <fstream>
#include <phttp.h>
#include <unzip.h>
#include <steam.h>
#include "cmd.h"
#include <thread>

static const char* GITHUB_API_URL = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases";

/**
 * @brief Unload and release the library from memory.
 * @param hinstDLL The handle to the library.
 */
const void UnloadAndReleaseLibrary(HINSTANCE hinstDLL) 
{
    FreeLibraryAndExitThread(hinstDLL, 0);
}

/**
 * @brief Get the current version of the updater module (this module). This is used to check 
 * compatibility with the latest version of Millennium, and th updater to make sure they are in sync.
 * @return Current version in semantic versioning format.
 */
extern "C" __attribute__((dllexport)) const char* __get_shim_version(void)
{
    return MILLENNIUM_VERSION;
}

/**
 * @brief Download the latest asset from the Millennium GitHub repository.
 * @param queuedDownloadUrl The URL to download the asset from.
 * @param steam_path The path to the Steam directory.
 */
void DownloadLatestAsset(std::string queuedDownloadUrl, std::string steam_path) 
{
    Print("Downloading asset: {}", queuedDownloadUrl);
    const std::string localDownloadPath = (std::filesystem::temp_directory_path() / "millennium.zip").string();

    if (DownloadResource(queuedDownloadUrl, localDownloadPath)) 
    { 
        Print("Successfully downloaded asset...");

        ExtractZippedArchive(localDownloadPath.c_str(), steam_path.c_str()); 
        remove(localDownloadPath.c_str());
    }
    else 
    {
        Error("Failed to download asset: {}", queuedDownloadUrl);
    }
}

/**
 * @brief Check for updates based off the update.flag file in the Steam directory.
 * Updates are not automatically derived from the server, they are specifically read from what the user has requested in the update.flag file.
 * the update.flag file is written to by assets\src\custom_components\UpdaterModal.tsx
 */
const void CheckForUpdates(std::string strSteamPath) 
{
    const auto start = std::chrono::high_resolution_clock::now();

    Print("Checking for updates...");
    Print("Steam path: {}", strSteamPath);

    const auto updateFlagPath = std::filesystem::path(strSteamPath) / "ext" / "update.flag";

    if (!std::filesystem::exists(updateFlagPath)) 
    {   
        Print("No updates were queried...");
        return;
    }

    std::ifstream updateFlagFile(updateFlagPath);
    std::string updateFlagContent((std::istreambuf_iterator<char>(updateFlagFile)), std::istreambuf_iterator<char>());
    updateFlagFile.close();

    if (std::remove(updateFlagPath.string().c_str()) != 0) 
    {
        Error("Failed to remove update.flag file...");
    }

    Print("Updating Millennium from queue: {}", updateFlagContent);
    DownloadLatestAsset(updateFlagContent, strSteamPath);  

    Print("Elapsed time: {}s", (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start)).count());
}

/**
 * @brief Enable virtual terminal processing for the console. 
 * Windows by default not support ANSI escape codes, this function enables it.
 */
void EnableVirtualTerminalProcessing() 
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return;
    }
}

/**
 * @brief Patch the SharedJSContext file to prevent the Steam UI from loading.
 * This is done by writing an empty document to the file.
 * We do this to prevent the Steam UI from loading before Millennium has started.
 */
void PatchSharedJSContext(std::string strSteamPath) 
{
    try 
    {
        const auto SteamUIModulePath       = std::filesystem::path(strSteamPath) / "steamui" / "index.html";
        Print("Patching SharedJSContext...");
        
        // Write an empty document to the file, this halts the Steam UI from loading as we've removed the <script>'s
        std::ofstream SteamUI(SteamUIModulePath, std::ios::trunc);
        SteamUI << "<!doctype html><html><head><title>SharedJSContext</title></head></html>";
        SteamUI.close();
    }
    catch (const std::system_error &error) 
    {
        Error("Caught system_error while patching SharedJSContext: {}", error.what());
    }
    catch (const std::exception &exception) 
    {
        Error("Error patching SharedJSContext: {}", exception.what());
    }
}

void ShowErrorMessage(DWORD errorCode)
{
    char errorMsg[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode, 0, errorMsg, sizeof(errorMsg), nullptr);
    
    std::string errorMessage = "Failed to load Millennium (millennium.dll). Error code: " + std::to_string(errorCode) + "\n" + errorMsg;
    MessageBoxA(nullptr, errorMessage.c_str(), "Error", MB_ICONERROR);
}

/**
 * @brief Allocate a console window for debugging purposes.
 * This is only called if the -dev argument is passed to the application.
 */
void AllocateDevConsole()
{
    std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

    if (startupParams->HasArgument("-dev")) 
    {
        (void)static_cast<bool>(AllocConsole());

        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        EnableVirtualTerminalProcessing();
    }
}

/**
 * @brief Unload Millennium from process memory.
 */
void UnloadMillennium()
{
    // Check if millennium.dll is already loaded in process memory, and if so, unload it.
    HMODULE hLoadedMillennium = GetModuleHandleA("millennium.dll");

    if (hLoadedMillennium != nullptr) 
    {
        Print("Unloading millennium.dll and reloading...");
        FreeLibrary(hLoadedMillennium);
    }
}

/**
 * @brief Load Millennium into process memory. 
 * 
 * This calls DllMain in %root%/src/main.cc which initializes Millennium.
 */
void LoadMillennium()
{
    // Check if the load was successful.
    if (LoadLibraryA("millennium.dll") == nullptr) 
    {
        DWORD errorCode = GetLastError();
        ShowErrorMessage(errorCode);
        return;
    }

    Print("Loaded millennium...");
}

const void BootstrapMillennium(HINSTANCE hinstDLL) 
{
    const std::string strSteamPath = GetSteamPath();
    AllocateDevConsole();

    Print("Starting Bootstrapper@{}", MILLENNIUM_VERSION);

    PatchSharedJSContext(strSteamPath);
    CheckForUpdates(strSteamPath);

    Print("Finished checking for updates");  

    LoadMillennium();
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)UnloadAndReleaseLibrary, hinstDLL, 0, nullptr);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) 
{
    if (fdwReason == DLL_PROCESS_ATTACH) 
    {
        // Detach from the main thread as to not wakeup the watchdog from Steam's debugger. 
        std::thread(BootstrapMillennium, hinstDLL).detach();
    }
    
    return true;
}