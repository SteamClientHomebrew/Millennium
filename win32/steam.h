#include <wtypes.h>
#include <string>

std::string get_steam_path() {
    // get the steam path from registry
    HKEY hKey;
    LONG lRes = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        return "C:/Program Files (x86)/Steam";
    }

    char buffer[MAX_PATH];
    DWORD bufferSize = sizeof(buffer);
    lRes = RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)buffer, &bufferSize);
    if (lRes != ERROR_SUCCESS) {
        return "C:/Program Files (x86)/Steam";
    }

    RegCloseKey(hKey);
    return std::string(buffer, bufferSize);
}