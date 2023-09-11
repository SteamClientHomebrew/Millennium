#include <utils/steam/steam_helpers.hpp>
#include <windows.h>

#include <vector>
#include <iostream>

// Interact with external proc
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

std::string Steam::getInstallPath() {
	std::string steamPath;
	HKEY hKey;
	LPCSTR subKey = "Software\\Valve\\Steam";

	if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		CHAR value[MAX_PATH];
		DWORD bufferSize = sizeof(value);

		if (RegGetValueA(hKey, nullptr, "SteamPath", RRF_RT_REG_SZ, nullptr, value, &bufferSize) == ERROR_SUCCESS) {
			steamPath = value;
		}
		RegCloseKey(hKey);
	}
	return steamPath;
}

bool Steam::isRunning() {
	std::vector<DWORD> pids(1024);
	DWORD bytesReturned;

	if (EnumProcesses(pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), &bytesReturned))
	{
		for (DWORD i = 0; i < (DWORD)(bytesReturned / sizeof(DWORD)); i++) {

			if (pids[i] == NULL)
				continue;

			HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pids[i]);

			if (hProcess == nullptr)
				continue;

			char processNameBuffer[MAX_PATH];

			if (!(GetModuleBaseNameA(hProcess, nullptr, processNameBuffer, sizeof(processNameBuffer)) > 0))
				continue;

			if (std::string(processNameBuffer) == "steam.exe")
			{
				std::cout << "[FATAL] Steam is running. Close it to continue" << std::endl;
				CloseHandle(hProcess);
				return true;
			}

			CloseHandle(hProcess);
		}
	}
	return false;
}
