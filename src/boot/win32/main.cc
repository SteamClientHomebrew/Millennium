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
#include <windows.h>
#include <string>
#include <shlwapi.h>

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

void ShowErrorMessage(DWORD errorCode)
{
    char errorMsg[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode, 0, errorMsg, sizeof(errorMsg), nullptr);

    std::string errorMessage = "Failed to load Millennium (millennium.dll). Error code: " + std::to_string(errorCode) + "\n" + errorMsg;
    MessageBoxA(nullptr, errorMessage.c_str(), "Error", MB_ICONERROR);
}

/**
 * @brief Load Millennium into process memory.
 *
 * This calls DllMain in %root%/src/main.cc which initializes Millennium.
 */
void LoadMillennium()
{
    const HMODULE hModule = LoadLibraryA("millennium.dll");

    if (!hModule) {
        ShowErrorMessage(GetLastError());
    }
}

const void BootstrapMillennium(HINSTANCE hinstDLL)
{
    LoadMillennium();
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)UnloadAndReleaseLibrary, hinstDLL, 0, nullptr);
}

BOOL IsSteamClient(VOID)
{
    CHAR path[MAX_PATH];

    if (!GetModuleFileNameA(NULL, path, MAX_PATH))
        return FALSE;

    return !_stricmp(PathFindFileNameA(path), "steam.exe");
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (!IsSteamClient()) {
        return TRUE;
    }

    if (fdwReason == DLL_PROCESS_ATTACH) {
        BootstrapMillennium(hinstDLL);
    }

    return true;
}