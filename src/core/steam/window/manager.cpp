#pragma once
#include "manager.hpp"

#include <stdafx.h>
#include <format>
#include <iostream>
#include <regex>
#include <windows.h>
#include <tlhelp32.h>

#include "types/system_backdrop_type.hpp"
#include "types/corners.hpp"
#include "types/accent_api.hpp"

#include <dwmapi.h>
#include <Psapi.h>
#include <Shlwapi.h>
#include <utils/config/config.hpp>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

OSVERSIONINFOEX os;


void EnableBlurBehind(HWND target)
{
	ACCENTPOLICY policy;
	policy.nAccentState = ACCENT_ENABLE_BLURBEHIND;
	policy.nFlags = ACCENT_FLAG_ENABLE_BLURBEHIND;
	policy.nColor = 0x00000000;
	policy.nAnimationId = 0;

	// Set the window composition attribute data
	WINDOWCOMPOSITIONATTRIBDATA data;
	data.nAttribute = 19; // WCA_ACCENT_POLICY
	data.pData = &policy;
	data.ulDataSize = sizeof(policy);

	HMODULE hUser32 = LoadLibrary("user32.dll");

	if (!hUser32) 
	{
		console.err("Failed to load user32.dll");
		return;
	}
	pSetWindowCompositionAttribute SetWindowCompositionAttribute = (pSetWindowCompositionAttribute)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
	if (!SetWindowCompositionAttribute) 
	{
		console.err("Failed to get function pointer for SetWindowCompositionAttribute");
		FreeLibrary(hUser32);
		return;
	}
	HRESULT hr = SetWindowCompositionAttribute(target, &data);
	if (FAILED(hr)) {
		console.err(std::format("SetWindowCompositionAttribute failed with error code: {}", hr));
		FreeLibrary(hUser32);
		return;
	}

	FreeLibrary(hUser32);
}


//struct DWMCOLORIZATIONPARAMS {
//	DWORD dwColor;
//	DWORD dwAfterglow;
//	DWORD dwColorBalance;
//	DWORD dwAfterglowBalance;
//	DWORD dwBlurBalance;
//	DWORD dwGlassReflectionIntensity;
//	DWORD dwOpaqueBlend;
//};
//
//struct IMMERSIVE_COLOR_PREFERENCE {
//	COLORREF color1;
//	COLORREF color2;
//};
//
//static HRESULT(WINAPI* DwmSetColorizationParameters)(DWMCOLORIZATIONPARAMS* color, UINT unknown);
//
//void SetWindowBorder(HWND hwnd) {
//	HMODULE hDwmApi = LoadLibrary("dwmapi.dll");
//	if (hDwmApi == NULL) {
//		std::cout << "Failed to load dwmapi.dll" << std::endl;
//		return;
//	}
//
//	auto pfnSetColorizationParams = reinterpret_cast<decltype(DwmSetColorizationParameters)>(GetProcAddress(hDwmApi, (LPCSTR)131));
//
//	if (pfnSetColorizationParams == NULL) {
//		std::cout << "Failed to get address of DwmSetColorizationParameters" << std::endl;
//		FreeLibrary(hDwmApi);
//		return;
//	}
//	else {
//		std::cout << "Found DwmSetColorizationParameters" << std::endl;
//	}
//
//	HRESULT hr;
//
//	DWMCOLORIZATIONPARAMS dwmColor;
//
//	DWORD dwNewColor = (((0xC4) << 24) | ((GetRValue(0x0000B9FF)) << 16) | ((GetGValue(0x0000B9FF)) << 8) | (GetBValue(0x0000B9FF)));
//	dwmColor.dwColor = dwNewColor;
//	dwmColor.dwAfterglow = dwNewColor;
//
//	hr = pfnSetColorizationParams(&dwmColor, 0);
//
//	if (FAILED(hr)) {
//		std::cout << "DwmSetColorizationParameters failed with HRESULT 0x" << std::hex << hr << std::endl;
//		FreeLibrary(hDwmApi);
//		return;
//	}
//
//	// Release the loaded library
//	FreeLibrary(hDwmApi);
//	return;
//}

void patchWindow(HWND hwnd)
{
	int enable_dark_mode = true;
	int SYSTEMBACKDROP_TYPE = (int)SystemBackdropType::Acrylic;

	CornerPreference pref;
	int cornerPref = Settings::Get<int>("corner-preference");
	int enableMica = Settings::Get<bool>("mica-effect");

	switch (cornerPref) {
		case 1: pref = CornerPreference::Rounded; break;
		case 2: pref = CornerPreference::Square; break;
		case 3: pref = CornerPreference::RoundedSmall; break;
	}

	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable_dark_mode, sizeof(int));
	DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &SYSTEMBACKDROP_TYPE, sizeof(int));

	if (cornerPref != NULL) {
		DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(int));
	}
	if (enableMica) {
		EnableBlurBehind(hwnd);
	}

	//SetWindowBorder(hwnd);
}

std::string GetProcName(DWORD processId) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (hProcess) {
		CHAR processName[MAX_PATH];
		if (GetProcessImageFileNameA(hProcess, processName, MAX_PATH) > 0) {
			CHAR* fileName = PathFindFileNameA(processName);
			return fileName;
		}
		CloseHandle(hProcess);
	}
	return "";
}

void CALLBACK OnEvent(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
	LONG idObject, LONG idChild, DWORD idEventThread, DWORD time)
{
	DWORD pid = 0;
	hwnd&& ::GetWindowThreadProcessId(hwnd, &pid);

	if (GetProcName(pid) == "steamwebhelper.exe") {

		if (event != EVENT_OBJECT_HIDE) {
			patchWindow(hwnd);
		}
	}
}

DWORD GetProcessIdFromName(const std::wstring& processName) {
	DWORD pid = 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		console.err("Failed to create a snapshot");
		return 0;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hSnapshot, &pe32)) {
		do {
			std::string narrowProcessName(processName.begin(), processName.end());
			if (_stricmp(pe32.szExeFile, narrowProcessName.c_str()) == 0) {
				pid = pe32.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return pid;
}

void GetAllWindowsFromProcessID(DWORD dwProcessID, std::vector <HWND>& vhWnds)
{
	HWND hCurWnd = NULL;
	do
	{
		hCurWnd = FindWindowEx(NULL, hCurWnd, NULL, NULL);
		DWORD dwProcID = 0;
		GetWindowThreadProcessId(hCurWnd, &dwProcID);
		if (dwProcID == dwProcessID)
		{
			vhWnds.push_back(hCurWnd);
		}
	} while (hCurWnd != NULL);
}

void updateHook()
{
	DWORD proccessId = GetProcessIdFromName(L"steamwebhelper.exe");

	std::vector<HWND> windows;
	GetAllWindowsFromProcessID(proccessId, windows);

	for (const auto& window : windows) {
		patchWindow(window);
	}
}

unsigned long __stdcall StartWinHookAsync(void*) {

	auto hHook = ::SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, nullptr, OnEvent, 0, 0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS | WINEVENT_SKIPOWNTHREAD);

	::GetMessageA(nullptr, nullptr, 0, 0);

	return true;
}

