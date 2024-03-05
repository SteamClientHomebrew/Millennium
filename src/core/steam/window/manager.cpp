#ifdef _WIN32
#include <stdafx.h>
#include <iostream>
#include <regex>
#include <windows.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#include <Psapi.h>
#include <Shlwapi.h>
#include <utils/config/config.hpp>

#include "manager.hpp"

#include "types/system_backdrop_type.hpp"
#include "types/corners.hpp"
#include "types/accent_api.hpp"
#include <format>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

OSVERSIONINFOEX os;

/**
 * @brief Enables blur effect behind the specified window.
 *
 * This function enables the blur effect behind the specified window by setting
 * the ACCENTPOLICY and WINDOWCOMPOSITIONATTRIBDATA attributes.
 *
 * @param target Handle to the window for which the blur effect is to be enabled.
 */
void EnableBlurBehind(HWND target)
{
	// Define the ACCENTPOLICY structure with blur effect settings.
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

	// Load user32.dll library.
	HMODULE hUser32 = LoadLibrary("user32.dll");

	if (!hUser32) 
	{
		console.err("Failed to load user32.dll");
		return;
	}

	// Get the function pointer for SetWindowCompositionAttribute.
	pSetWindowCompositionAttribute SetWindowCompositionAttribute = (pSetWindowCompositionAttribute)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
	
	if (!SetWindowCompositionAttribute) 
	{
		console.err("Failed to get function pointer for SetWindowCompositionAttribute");
		FreeLibrary(hUser32);
		return;
	}

	// Call SetWindowCompositionAttribute to enable blur effect.
	HRESULT hr = SetWindowCompositionAttribute(target, &data);

	if (FAILED(hr)) {
		console.err(fmt::format("SetWindowCompositionAttribute failed with error code: {}", hr));
		FreeLibrary(hUser32);
		return;
	}

	FreeLibrary(hUser32);
}

// Structure representing colorization parameters for DWM.
struct DWMCOLORIZATIONPARAMS {
    DWORD dwColor;                    // Color of window frame.
    DWORD dwAfterglow;                // Color of window border glow.
    DWORD dwColorBalance;             // Color balance.
    DWORD dwAfterglowBalance;         // Afterglow balance.
    DWORD dwBlurBalance;              // Blur balance.
    DWORD dwGlassReflectionIntensity; // Glass reflection intensity.
    DWORD dwOpaqueBlend;              // Opaque blend.
};

// Structure representing immersive color preference.
struct IMMERSIVE_COLOR_PREFERENCE {
    COLORREF color1;  // First color preference.
    COLORREF color2;  // Second color preference.
};

/**
 * @brief Sets the colorization parameters for DWM (Desktop Window Manager).
 *
 * This function takes a pointer to a DWMCOLORIZATIONPARAMS structure containing colorization
 * parameters and sets them using the DwmSetColorizationParameters function.
 *
 * @param color Pointer to a DWMCOLORIZATIONPARAMS structure containing colorization parameters.
 * @param unknown Unknown parameter (set to 0).
 * @return HRESULT indicating success or failure of the operation.
 */
static HRESULT(WINAPI* DwmSetColorizationParameters)(DWMCOLORIZATIONPARAMS* color, UINT unknown);


/**
 * @brief Sets the window border color using DWM APIs.
 *
 * This function loads the dwmapi.dll library and retrieves the address of the
 * DwmSetColorizationParameters function. It then sets the window border color
 * using the specified color parameters.
 *
 * @param hwnd Handle to the window for which the border color is to be set.
 */
void SetWindowBorder(HWND hwnd) 
{	

	/**
	 * @brief Load dwmapi and find the 131th table export from the linked library
	 * this is an undocumented function that is not part of the windows api, however it is fully supported
	 * on windows 11 natively. 
	 */

	HMODULE hDwmApi = LoadLibrary("dwmapi.dll");
	if (hDwmApi == NULL) {
		std::cout << "Failed to load dwmapi.dll" << std::endl;
		return;
	}

	auto pfnSetColorizationParams = reinterpret_cast<decltype(DwmSetColorizationParameters)>(GetProcAddress(hDwmApi, (LPCSTR)131));

	if (pfnSetColorizationParams == NULL) {
		std::cout << "Failed to get address of DwmSetColorizationParameters" << std::endl;
		FreeLibrary(hDwmApi);
		return;
	}
	else {
		std::cout << "Found DwmSetColorizationParameters" << std::endl;
	}

	HRESULT hr;
	DWMCOLORIZATIONPARAMS dwmColor;

	// bit shift the color from rgb to an unsigned long
	DWORD dwNewColor = (((0xC4) << 24) | ((GetRValue(0x0000B9FF)) << 16) | ((GetGValue(0x0000B9FF)) << 8) | (GetBValue(0x0000B9FF)));
	dwmColor.dwColor = dwNewColor;
	dwmColor.dwAfterglow = dwNewColor;

	hr = pfnSetColorizationParams(&dwmColor, 0);

	if (FAILED(hr)) {
		std::cout << "DwmSetColorizationParameters failed with HRESULT 0x" << std::hex << hr << std::endl;
		FreeLibrary(hDwmApi);
		return;
	}

	// Release the loaded library
	FreeLibrary(hDwmApi);
	return;
}

/**
 * @brief Applies patches to the specified window.
 *
 * This function applies various visual patches to the specified window, such as enabling
 * dark mode, setting window corner preferences, and enabling blur effects (if applicable).
 *
 * @param hwnd Handle to the window to be patched.
 */
void patchWindow(HWND hwnd)
{
	// Enable dark mode for the window.
	int enable_dark_mode = true;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable_dark_mode, sizeof(int));

	// Retrieve corner preference and blur effect settings from application settings.
	int cornerPref = Settings::Get<int>("corner-preference");
	int enableMica = Settings::Get<bool>("mica-effect");

	// Initialize corner preference with default value.
	CornerPreference pref = CornerPreference::Rounded;

	// Map corner preference setting to appropriate enum value.
	switch (cornerPref) {
	case 1: pref = CornerPreference::Rounded; break;
	case 2: pref = CornerPreference::Square; break;
	case 3: pref = CornerPreference::RoundedSmall; break;
	}

	// Set window corner preference if specified.
	if (cornerPref != 0) {
		DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(int));
	}

	// Enable blur effects if applicable.
	if (enableMica) {
		EnableBlurBehind(hwnd);
	}
}

/**
 * @brief Retrieves the name of the process associated with the given process ID.
 *
 * This function takes a process ID as input and retrieves the name of the corresponding
 * process. If successful, it returns the process name as a string; otherwise, it returns
 * an empty string.
 *
 * @param processId The process ID of the target process.
 * @return The name of the process, or an empty string if the process name cannot be retrieved.
 */
std::string GetProcName(DWORD processId) {
	// Open the process with PROCESS_QUERY_LIMITED_INFORMATION access rights.
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!hProcess) return {};

	char processName[MAX_PATH];
	bool success = GetProcessImageFileNameA(hProcess, processName, MAX_PATH) > 0;

	// Close the handle to the process.
	CloseHandle(hProcess);
	if (!success) return {};

	char* fileName = PathFindFileNameA(processName);

	// Return the file name as the process name.
	return fileName;
}

/**
 * @brief Callback function invoked when a WinEvent is triggered.
 *
 * This function is called when a WinEvent, such as EVENT_OBJECT_CREATE or EVENT_OBJECT_SHOW,
 * occurs. It receives information about the event, including the event type, the window handle
 * associated with the event, and additional parameters. Depending on the event type and the
 * associated process, it may apply patches to the window.
 *
 * @param hWinEventHook Handle to the WinEvent hook.
 * @param event The type of WinEvent that occurred.
 * @param hwnd Handle to the window associated with the event.
 * @param idObject Identifier of the object associated with the event.
 * @param idChild Identifier of the child object associated with the event.
 * @param idEventThread Identifier of the thread that generated the event.
 * @param time Time at which the event occurred.
 */
void CALLBACK OnEvent(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
	LONG idObject, LONG idChild, DWORD idEventThread, DWORD time)
{
	// Get the process ID associated with the window.
	DWORD pid = 0;
	hwnd&& ::GetWindowThreadProcessId(hwnd, &pid);

	// Check if the process associated with the window is "steamwebhelper.exe".
	if (GetProcName(pid) == "steamwebhelper.exe") {
		// Apply patches to the window if the event is not EVENT_OBJECT_HIDE.
		if (event != EVENT_OBJECT_HIDE) {
			patchWindow(hwnd);
		}
	}
}
/**
 * @brief Retrieves the process ID associated with a given process name.
 *
 * This function takes a process name as input and searches for a process
 * with a matching name. If found, it returns the process ID; otherwise,
 * it returns 0.
 *
 * @param processName The name of the process to search for.
 * @return The process ID of the found process, or 0 if the process is not found.
 */
DWORD GetProcessIdFromName(const std::string& processName) {
	DWORD pid = 0;

	// Create a snapshot of the system processes.
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		console.err("Failed to create a snapshot");
		return 0;
	}

	// Iterate through the processes in the snapshot.
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hSnapshot, &pe32)) {
		do {

			// Compare the process name (case-insensitive) with the name of the current process.
			if (_stricmp(pe32.szExeFile, processName.c_str()) == 0) {
				pid = pe32.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));
	}

	// Close the handle to the snapshot.
	CloseHandle(hSnapshot);

	// Return the process ID.
	return pid;
}

/**
 * @brief Retrieves all windows associated with a specific process ID.
 *
 * This function iterates through all top-level windows to find those
 * belonging to the specified process ID. It adds the found windows
 * to the provided vector.
 *
 * @param dwProcessID The process ID of the target process.
 * @param vhWnds A vector to store the handles of windows associated with the specified process.
 */
void GetAllWindowsFromProcessID(DWORD dwProcessID, std::vector<HWND>& vhWnds)
{
	HWND hCurWnd = NULL;
	do
	{
		// Find the next top-level window.
		hCurWnd = FindWindowEx(NULL, hCurWnd, NULL, NULL);

		// Get the process ID associated with the current window.
		DWORD dwProcID = 0;
		GetWindowThreadProcessId(hCurWnd, &dwProcID);

		// If the process ID matches the target process ID, add the window handle to the vector.
		if (dwProcID == dwProcessID)
		{
			vhWnds.push_back(hCurWnd);
		}
	} while (hCurWnd != NULL);
}

/**
 * @brief Updates hooks for windows associated with a specific process.
 *
 * This function retrieves the process ID of a target process by name,
 * retrieves all windows associated with that process, and applies the
 * necessary patches to each window.
 */
void updateHook()
{
	// Get the process ID of the target process by name (e.g., "steamwebhelper.exe").
	DWORD processId = GetProcessIdFromName("steamwebhelper.exe");

	// Retrieve all windows associated with the target process.
	std::vector<HWND> windows;
	GetAllWindowsFromProcessID(processId, windows);

	// Apply patches to each window.
	for (const auto& window : windows) {
		patchWindow(window);
	}
}

/**
 * @brief Initiates a Windows event hook asynchronously to listen for specific
 *        events related to object creation and showing on the desktop. Upon
 *        triggering the specified events, it invokes the provided callback function OnEvent.
 *
 * @param lpParam A pointer to an optional parameter. This parameter is not used within the function.
 *
 * @return true if the hook is successfully initiated; otherwise, the return value is not defined.
 */
unsigned long __stdcall StartWinHookAsync(void* lpParam) {

	// Set up a WinEvent hook to listen for EVENT_OBJECT_CREATE and EVENT_OBJECT_SHOW events.
	// When either of these events occur, the provided callback function OnEvent will be invoked.
	auto hHook = ::SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, nullptr, OnEvent, 0, 0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS | WINEVENT_SKIPOWNTHREAD);

	// Enter a message loop to keep the hook running asynchronously.
	// This ensures that the system can process messages and events while the hook remains active.
	::GetMessageA(nullptr, nullptr, 0, 0);

	// Return true to indicate that the hook was successfully initiated.
	return true;
}
#endif