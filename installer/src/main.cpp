#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>

#include <window/memory.h>
#include <window/window.hpp>
#include <vendor/imgui/imgui_internal.h>
#define Region ImGui::GetContentRegionAvail()
#define VW ImGui::GetWindowWidth()
#define VH ImGui::GetWindowHeight()

#include <psapi.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <DbgHelp.h>

#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include <Shlwapi.h>
#include <strsafe.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <TlHelp32.h>

std::stringstream standard_out;

static bool install_success = true;
static bool uninstall_success = true;

static int current_page = 1;
static bool accepted_tos;
static bool show_accept_tos_popup = false;
static int selected_option = 0;

const char* repo_url = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";
bool is_auto_installer = static_cast<std::string>(GetCommandLineA()).find("-update") != std::string::npos;
static float progress;

int download_iteration = 0;
int downloads_length = 0;

static std::string current_name;


std::string get_steam_install_path()
{
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

class downloader {
private:
	char buffer[8096] = {};
	DWORD total_bytes_read = {};
	std::basic_string<char, std::char_traits<char>, std::allocator<char>> github_response;

	HINTERNET request_buffer, connection;
public:
	downloader() {
		connection = InternetOpenA("millennium.installer", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		request_buffer = InternetOpenUrlA(connection, repo_url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

		if (!request_buffer || !connection) {
			std::cout << "Error opening URL: " << GetLastError() << std::endl;
			InternetCloseHandle(connection);
			return;
		}
	}

	inline std::string get_github()
	{
		while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read > 0) {
			github_response.append(buffer, total_bytes_read);
		}

		return github_response;
	}

	inline void download_millennium()
	{
		nlohmann::json response = nlohmann::json::parse(github_response);

		if (response.contains("message") && response["message"].get<std::string>() == "Not Found")
		{
			std::cout << "Couldn't get latest release because it doesn't exist. Try again later?" << std::endl;
			return;
		}

		std::string tag_name = response["tag_name"].get<std::string>();
		std::string notes = response["body"].get<std::string>();

		std::cout << "Getting latest release of millennium: v" << tag_name << std::endl;
		std::cout << "Patch notes:\n" << notes << std::endl;
		std::cout << "Evaluating " << response["assets"].size() << " files to download" << std::endl;

		downloads_length = response["assets"].size();

		for (nlohmann::basic_json<>& item : response["assets"])
		{
			download_iteration++;

			auto startTime = std::chrono::high_resolution_clock::now();

			current_name = item["name"].get<std::string>();

			std::string name = item["name"].get<std::string>();
			std::string download = item["browser_download_url"].get<std::string>();

			double size = static_cast<double>(item["size"].get<int>()) / 1048576.0;
			int download_count = item["download_count"].get<int>();

			std::cout << "--------------------------------" << std::endl;

			std::cout << "Downloading: [" << name << "] " << std::endl;
			std::cout << "File size: [" << size << "mb]" << std::endl;
			std::cout << "Download Count: [" << download_count << "]" << std::endl;

			HINTERNET hConnection = InternetOpenUrlA(connection, download.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

			std::string path = get_steam_install_path() + "/" + name;

			std::cout << "Downloading to: " << path << std::endl;

			std::remove(path.c_str());

			HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			DWORD bytesRead;
			BYTE buffer[1024];
			DWORD totalBytesRead = 0;
			DWORD totalBytesExpected = item["size"].get<int>();

			while (InternetReadFile(hConnection, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
				DWORD bytesWritten;
				WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL);
				totalBytesRead += bytesRead;

				progress = static_cast<float>(totalBytesRead) / static_cast<float>(totalBytesExpected);
			}

			CloseHandle(hFile);

			auto endTime = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

			std::cout << "Done! " << duration << " milliseconds elapsed" << std::endl;
		}

		std::cout << "--------------------------------" << std::endl;
		std::cout << "Installation completed. You can now start Steam" << std::endl;

		if (is_auto_installer)
		{
			STARTUPINFO si = { sizeof(si) };
			PROCESS_INFORMATION pi;

			if (CreateProcess(NULL, const_cast<char*>(std::format("{}/steam.exe", get_steam_install_path()).c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				exit(1);
			}
		}
	}

	~downloader()
	{
		InternetCloseHandle(request_buffer);
		InternetCloseHandle(connection);
	}
};

downloader download;

static nlohmann::basic_json<> github_data = nlohmann::json::parse(download.get_github());

bool steam_running()
{
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

void uninstall_millennium()
{
	if (steam_running()) {
		uninstall_success = false;
		return;
	}
	else uninstall_success = true;

	std::string directory = get_steam_install_path();;
	std::vector<std::string> filesToDelete = { "User32.dll", "libcrypto-3.dll" };

	for (const auto& file : filesToDelete)
	{
		std::filesystem::path filePath = directory + "/" + file;
		if (std::filesystem::exists(filePath)) {
			if (std::filesystem::remove(filePath)) {
				std::cout << "[SUCCESS] Deleted file: " << filePath << std::endl;
			}
			else {
				std::cout << "[FATAL] Failed to delete file: " << filePath << std::endl;
				uninstall_success = false;
			}
		}
		else {
			std::cout << "[WARNING] File does not exist: " << filePath << std::endl;
		}
	}
}

DWORD __stdcall install_millennium(void*)
{
	if (steam_running()) {
		install_success = false;
		return false;
	}
	else install_success = true;

	download.download_millennium();
}

void reset_millennium()
{
	if (steam_running()) {
		return;
	}

	std::string steamPath = get_steam_install_path();
	std::string skinsFolderPath = steamPath + "\\steamui\\skins";

	try {
		std::filesystem::remove_all(skinsFolderPath);
		std::cout << "[SUCESS] Successfully reset the millennium client" << std::endl;
	}
	catch (std::filesystem::filesystem_error& e) {
		std::cout << "[FATAL] Failed to reset the millennium client" << e.what() << std::endl;
	}
}

std::string terms_and_conditions;
const char* terms_address = "https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/.terms-of-service";

class terms_of_service {
private:
	char buffer[4096] = {};
	DWORD total_bytes_read = {};
	std::basic_string<char, std::char_traits<char>, std::allocator<char>> github_response;

	HINTERNET request_buffer, connection;
public:
	terms_of_service() {
		connection = InternetOpenA("millennium", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		request_buffer = InternetOpenUrlA(connection, terms_address, NULL, 0, INTERNET_FLAG_RELOAD, 0);

		if (!request_buffer || !connection) {
			std::cout << "Error opening URL: " << GetLastError() << std::endl;
			InternetCloseHandle(connection);
			return;
		}
	}

	inline std::string get_tos() {
		while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read > 0) { github_response.append(buffer, total_bytes_read); }

		if (github_response.empty()) {
			return "Couldn't get Terms of Service, do NOT finish this installation without reading the TOS";
		}
		return github_response;
	}

	~terms_of_service() {
		InternetCloseHandle(request_buffer);
		InternetCloseHandle(connection);
	}
};

class Handler
{
public:
	__declspec(noinline) static void tos_agreement_page(void)
	{
		if (ImGui::BeginChild("##agree", ImVec2(Region.x, Region.y - 30), true)) {
			ImGui::TextWrapped(terms_and_conditions.c_str());
		}
		ImGui::EndChild();

		ImGui::Checkbox("I accept the following", &accepted_tos);
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Next >").x - 3); // Align cursor to the right

		if (ImGui::Button("Next >")) {
			if (!accepted_tos) show_accept_tos_popup = true;
			else current_page = 2;
		}

		CenterModal(ImVec2(200, 125));
		const char* header_text = " Millennium - Error";

		if (show_accept_tos_popup) ImGui::OpenPopup(header_text);
		if (ImGui::BeginPopupModal(header_text, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			ImGui::TextWrapped("You need to agree with the terms of service to continue.");

			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 5);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetStyle().ItemSpacing.y - 23);

			if (ImGui::Button("Close")) {
				ImGui::CloseCurrentPopup();
				show_accept_tos_popup = false;
			}
			ImGui::EndPopup();
		}
	}

	__declspec(noinline) static void select_option_page(void)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10, 0.10, 0.10, 1.0f));
		if (ImGui::BeginChild("##install_page", ImVec2(Region.x, Region.y - 30), false))
		{
			static const char* install = "Install Millennium", * reset = "Reset Millennium", * uninstall = "Uninstall Millennium";
			ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

			ImGui::Text("Choose an option:");
			if (selected_option == 1) {
				ImGui::PushStyleColor(ImGuiCol_Button, col);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
				if (ImGui::Button(install, ImVec2(Region.x, 45))) selected_option = 1;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				ImGui::PopStyleColor(2);
			}
			else {
				if (ImGui::Button(install, ImVec2(Region.x, 45))) selected_option = 1;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			if (selected_option == 2) {
				ImGui::PushStyleColor(ImGuiCol_Button, col);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
				if (ImGui::Button(reset, ImVec2(Region.x, 45))) selected_option = 2;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				ImGui::PopStyleColor(2);
			}
			else {
				if (ImGui::Button(reset, ImVec2(Region.x, 45))) selected_option = 2;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			if (selected_option == 3)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, col);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
				if (ImGui::Button(uninstall, ImVec2(Region.x, 45))) selected_option = 3;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				ImGui::PopStyleColor(2);
			}
			else {
				if (ImGui::Button(uninstall, ImVec2(Region.x, 45))) selected_option = 3;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Next >").x - 53); // Align cursor to the right
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));

		if (ImGui::Button("< Back")) current_page = 1;
		ImGui::SameLine(0);
		if (ImGui::Button("Next >"))
		{
			current_page = 3;

			if (selected_option == 0)
			{
				ImGui::OpenPopup(" Choose an option");
				current_page = 2;
			}
			if (selected_option == 1)
			{
				standard_out.str("");
				std::cout << "[INFO] Starting installer..." << std::endl;
				CreateThread(0, 0, install_millennium, 0, 0, 0);
			}
			else if (selected_option == 2) {
				ImGui::OpenPopup(" Reset Warning");
				standard_out.str("");
				std::cout << "[INFO] Resetting Millennium..." << std::endl;
			}
			else if (selected_option == 3)
			{
				standard_out.str("");
				std::cout << "[INFO] Starting uninstaller..." << std::endl;
				uninstall_millennium();

				if (uninstall_success) {
					std::cout << "[SUCCESS] Successfully uninstalled Millennium from your computer!" << std::endl;
					std::cout << "[INFO] Thanks for supporting the project!" << std::endl;
					std::cout << "[INFO] You can now close this window..." << std::endl;
				}
				else {
					std::cout << "[FATAL] Uninstall failed, refer to the errors above.\n" << std::endl;
				}
			}
		}
		ImGui::PopStyleVar();
	}

	__declspec(noinline)static void console_page(void)
	{
		if (ImGui::BeginChild("##console", ImVec2(Region.x, Region.y - 30), true)) {
			ImGui::TextWrapped(standard_out.str().c_str());
		}
		ImGui::EndChild();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));

		if (ImGui::Button("Discord")) ShellExecuteA(nullptr, "open", "https://discord.gg/MXMWEQKgJF", nullptr, nullptr, SW_SHOWNORMAL);
		ImGui::SameLine();
		if (ImGui::Button("Github")) ShellExecuteA(nullptr, "open", "https://github.com/ShadowMonster99/millennium-steam-patcher", nullptr, nullptr, SW_SHOWNORMAL);

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 60); // Align cursor to the right

		if (ImGui::Button("< Back")) current_page = 2;
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			ShellExecuteA(nullptr, "open", "https://discord.gg/MXMWEQKgJF", nullptr, nullptr, SW_SHOWNORMAL);
			exit(1);
		}

		ImGui::PopStyleVar();
	}

	__declspec(noinline) static void __fastcall Render(void)
	{
		auto center = [](float avail_width, float element_width, float padding = 0) { ImGui::SameLine((avail_width / 2) - (element_width / 2) + padding); };
		auto center_text = [&](const char* format, float spacing = 15, ImColor color = ImColor(255, 255, 255)) { center(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(format).x); ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);	ImGui::TextColored(color.Value, format); };

		if (is_auto_installer)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10, 0.10, 0.10, 1.00f));

			if (ImGui::BeginChild("##left_side", ImVec2(Region.x / 2 - (ImGui::GetStyle().ItemSpacing.x / 2), Region.y), false))
			{
				center(ImGui::GetContentRegionAvail().x, 45);

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 45);

				ImGui::Image((void*)WindowClass::GetIcon(), ImVec2(45, 45));

				center_text(std::format("Millennium v{}", github_data["tag_name"].get<std::string>()).c_str(), 55);

				static int button_width = 125;

				center(ImGui::GetContentRegionAvail().x, button_width);

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 25);

				static bool is_downloading = false;

				if (ImGui::Button("Install", ImVec2(button_width, 23)))
				{
					is_downloading = true;

					std::make_shared<std::thread>([&] {
						download.download_millennium();
					})->detach();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Install the latest version of Millennium to Steam.\nKeeping Millennium up to date is required.");
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				}
				
				if (is_downloading)
				{
					ImGui::SetCursorPosY(VH - 25);

					if (ImGui::BeginChild("###download_box", ImVec2(Region.x, 25), false))
					{
						if (download_iteration == downloads_length)
						{
							ImGui::Text("Done! Cleaning up...  ");
							ImGui::SameLine();
							ImGui::Spinner("###loading", 5, 2, ImGui::GetColorU32(ImGuiCol_CheckMark));
						}
						else
						{
							ImGui::Text(std::format("[{} of {} files] {}", download_iteration, downloads_length, current_name).c_str());
						}
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

						ImGui::ProgressBar(progress, ImVec2(Region.x - 7, 4));

						ImGui::EndChild();
					}
				}

				ImGui::EndChild();
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();

			if (ImGui::BeginChild("##right_side", Region, true))
			{
				ImGui::Text("Patch Notes:");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
				ImGui::Separator();

				ImGui::TextWrapped(github_data["body"].get<std::string>().c_str());

				ImGui::EndChild();
			}

			ImGui::PopStyleVar();
		}
		else
		{
			if (current_page == 1)
				tos_agreement_page();
			else if (current_page == 2)
				select_option_page();
			else if (current_page == 3)
				console_page();

			CenterModal(ImVec2(200, 125));
			if (ImGui::BeginPopupModal(" Choose an option", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
			{
				ImGui::TextWrapped("You must choose an option to proceed.");

				ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 5);
				ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetStyle().ItemSpacing.y - 23);

				if (ImGui::Button("Close")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}


			CenterModal(ImVec2(215, 150));
			if (ImGui::BeginPopupModal(" Reset Warning", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
			{
				ImGui::TextWrapped("Resetting your settings will delete everything related to millennium. This means you skins will also be deleted.\n\nProceed?");

				ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Cancel").x - 65);
				ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetStyle().ItemSpacing.y - 23);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));

				if (ImGui::Button("Confirm")) {
					reset_millennium();
					ImGui::CloseCurrentPopup();
					show_accept_tos_popup = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					std::cout << "[INFO] Aborting reset..." << std::endl;
					ImGui::CloseCurrentPopup();
					show_accept_tos_popup = false;
				}
				ImGui::PopStyleVar();
				ImGui::EndPopup();
			}
		}	
	}
};

std::string GetStackTraceString()
{
	constexpr int maxFrames = 64;
	void* frames[maxFrames];

	// Capture the stack trace
	USHORT numFrames = CaptureStackBackTrace(0, maxFrames, frames, nullptr);

	// Load symbols for the captured addresses
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME);
	SymInitialize(GetCurrentProcess(), nullptr, TRUE);

	std::ostringstream oss;
	oss << "Stack Trace:" << std::endl;

	// Resolve symbols and append to the stack trace string
	for (USHORT i = 0; i < numFrames; ++i)
	{
		DWORD_PTR address = reinterpret_cast<DWORD_PTR>(frames[i]);
		CHAR symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;

		if (SymFromAddr(GetCurrentProcess(), address, nullptr, symbol))
		{
			oss << "[" << i << "] " << symbol->Name << "()" << std::endl;
		}
		else
		{
			oss << "[" << i << "] <symbol not found>" << std::endl;
		}
	}

	SymCleanup(GetCurrentProcess());

	return oss.str();
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		if (is_auto_installer)
		{
			const char* processName = "steam.exe";

			DWORD processId = 0;
			HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hProcessSnap != INVALID_HANDLE_VALUE)
			{
				PROCESSENTRY32 pe32;
				pe32.dwSize = sizeof(PROCESSENTRY32);

				if (Process32First(hProcessSnap, &pe32))
				{
					do {
						if (strcmp(pe32.szExeFile, processName) == 0)
						{
							processId = pe32.th32ProcessID;
							break;
						}
					} while (Process32Next(hProcessSnap, &pe32));
				}

				CloseHandle(hProcessSnap);
			}

			if (processId != 0)
			{
				HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
				if (hProcess != NULL)
				{
					TerminateProcess(hProcess, 0);
					CloseHandle(hProcess);
				}
			}

			WindowClass::WindowTitle({ (char*)u8"Installer.Millennium.Prod+v1.0.0" });
			WindowClass::WindowDimensions(ImVec2({ 450, 300 }));

			std::string title = "###";

			Application::Create(title, &Handler::Render);
		}
		else
		{
			std::streambuf* originalCoutBuffer = std::cout.rdbuf();
			std::cout.rdbuf(standard_out.rdbuf());

			terms_of_service terms;
			terms_and_conditions = terms.get_tos();

			WindowClass::WindowTitle({ (char*)"Millennium Installer" });
			WindowClass::WindowDimensions(ImVec2({ 450, 300 }));

			std::string title = "###";

			Application::Create(title, &Handler::Render);
		}
	}
	catch (std::exception& ex)
	{
		MessageBoxA(0, std::format("fatal error occured when trying to run the installer.\n\n[ERROR]{}\n\n[STACK:TRACE]{}\n\nTake a screen shot of the problem and report it in the server if you are stumped\nMy automatic guess would be the download api is unavailable or you dont have internet access", ex.what(), GetStackTraceString().c_str()).c_str(), "Millennium - ERROR", MB_ICONERROR);
	}

	return true;
}
