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

#include <steam.h>
#include <string>
#include <wtypes.h>

/**
 * @brief Get the path to the Steam installation, read from the registry.
 * @return The path to the Steam installation.
 */
std::string GetSteamPath()
{
    HKEY hiveKey;
    LONG registryOpenResult = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hiveKey);

    if (registryOpenResult != ERROR_SUCCESS) {
        return FALLBACK_STEAM_PATH;
    }

    char buffer[MAX_PATH];
    DWORD bufferSize = sizeof(buffer);
    registryOpenResult = RegQueryValueExA(hiveKey, "SteamPath", NULL, NULL, (LPBYTE)buffer, &bufferSize);

    if (registryOpenResult != ERROR_SUCCESS) {
        return FALLBACK_STEAM_PATH;
    }

    RegCloseKey(hiveKey);
    return std::string(buffer, strlen(buffer));
}