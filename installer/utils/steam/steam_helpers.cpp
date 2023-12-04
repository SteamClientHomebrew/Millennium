#include <utils/steam/steam_helpers.hpp>
#include <windows.h>

#include <vector>
#include <iostream>

#include <Psapi.h>
#include <TlHelp32.h>
#pragma comment(lib, "Psapi.lib")
#include <format>

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

void Steam::Start() {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	std::string applicationName = std::format("{}/steam.exe", Steam::getInstallPath());

	std::cout << applicationName << '\n';

	if (CreateProcess(applicationName.c_str(), 0, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
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

void Steam::terminateProcess()
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (Process32First(snapshot, &entry)) {
		do {
			if (_stricmp(entry.szExeFile, "Steam.exe") == 0) {
				HANDLE process = OpenProcess(PROCESS_TERMINATE, 0, entry.th32ProcessID);
				if (process != NULL) {
					TerminateProcess(process, 0);
					CloseHandle(process);
					std::cout << "Steam terminated successfully." << std::endl;
					return;
				}
				else {
					std::cerr << "Error opening Steam process for termination." << std::endl;
					return;
				}
			}
		} while (Process32Next(snapshot, &entry));
	}

	std::cerr << "Steam process not found." << std::endl;
}
