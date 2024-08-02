int ReloadSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "reload.flag");
    return 0;
}

int RestartSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "restart.flag");
    return 0;
}

int ForceRestartSteam() {
    std::ofstream(SystemIO::GetSteamPath() / "ext" / "restart_force.flag");
    return 0;
}

#ifdef _WIN32
std::string GetSteamPathFromRegistry() {
    HKEY hKey;
    char value[512];
    DWORD valueLength = sizeof(value);
    LONG result;

    result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        std::cerr << "Error opening registry key: " << result << std::endl;
        return {};
    }

    result = RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)value, &valueLength);
    if (result != ERROR_SUCCESS) {
        std::cerr << "Error reading registry value: " << result << std::endl;
        RegCloseKey(hKey);
        return {};
    }

    RegCloseKey(hKey);
    return std::string(value, valueLength - 1);
}
#endif