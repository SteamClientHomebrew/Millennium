#include <wtypes.h>
#include <string>
#include <steam.h>

std::string GetSteamPath() 
{
    HKEY hiveKey;
    LONG registryOpenResult = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hiveKey);

    if (registryOpenResult != ERROR_SUCCESS) 
    {
        return FALLBACK_STEAM_PATH;
    }

    char buffer[MAX_PATH];
    DWORD bufferSize = sizeof(buffer);
    registryOpenResult = RegQueryValueExA(hiveKey, "SteamPath", NULL, NULL, (LPBYTE)buffer, &bufferSize);

    if (registryOpenResult != ERROR_SUCCESS) 
    {
        return FALLBACK_STEAM_PATH;
    }

    RegCloseKey(hiveKey);
    return std::string(buffer, strlen(buffer));
}       