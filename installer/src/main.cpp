#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>

#include <window/memory.h>
#include <window/window.hpp>
#include <vendor/imgui/imgui_internal.h>

#define Region ImGui::GetContentRegionAvail()
#define VW ImGui::GetWindowWidth()
#define VH ImGui::GetWindowHeight()

#include <nlohmann/json.hpp>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")

#include <utils/steam/steam_helpers.hpp>
#include <src/installer/installer.hpp>
#include <utils/http/http_client.hpp>

std::stringstream standard_out;

static int current_page = 1;
static bool accepted_tos;
static bool show_accept_tos_popup = false;
static int selected_option = 0;

const char* repo_url = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";

std::string terms_and_conditions;

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
				CreateThread(0, 0, [](LPVOID lpParam) -> DWORD { 
					installer.installMillennium(); 
					return true;
				}, 0, 0, 0);
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
				CreateThread(0, 0, [](LPVOID lpParam) -> DWORD { 
					installer.uninstallMillennium(); 
					return true; 
				}, 0, 0, 0);
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

		if (ImGui::Button("Discord")) 
		{
			//ShellExecuteA(nullptr, "open", "https://discord.gg/MXMWEQKgJF", nullptr, nullptr, SW_SHOWNORMAL);
		}
		ImGui::SameLine();
		if (ImGui::Button("Github")) 
		{
			//ShellExecuteA(nullptr, "open", "https://github.com/ShadowMonster99/millennium-steam-patcher", nullptr, nullptr, SW_SHOWNORMAL);
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 60); // Align cursor to the right

		if (ImGui::Button("< Back")) current_page = 2;
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			//ShellExecuteA(nullptr, "open", "https://discord.gg/MXMWEQKgJF", nullptr, nullptr, SW_SHOWNORMAL);
			exit(1);
		}

		ImGui::PopStyleVar();
	}

	__declspec(noinline) static void Render(void)
	{
		auto center = [](float avail_width, float element_width, float padding = 0) { ImGui::SameLine((avail_width / 2) - (element_width / 2) + padding); };
		auto center_text = [&](const char* format, float spacing = 15, ImColor color = ImColor(255, 255, 255)) { center(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(format).x); ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);	ImGui::TextColored(color.Value, format); };

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
				installer.resetMillennium();
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
};

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try {
		//redirect std::cout events
		std::cout.rdbuf(standard_out.rdbuf());

		try {
			terms_and_conditions = http::get("https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/.terms-of-service");
		}
		catch (const http_error& error) {
			switch (error.code()) {
				case http_error::errors::couldnt_connect: {
					terms_and_conditions = 
						"Couldn't GET Millennium's TOS. Check the Github repository to see the TOS.\n"
						"Do not proceed the installer if you haven't read the TOS";
					break;
				}
			}
		}
		
		WindowClass::WindowTitle((char*)"Millennium Installer");
		WindowClass::WindowDimensions(ImVec2({ 450, 300 }));

		Application::Create(&Handler::Render);
	}
	catch (std::exception& ex) {
		MessageBoxA(0, ex.what(), "Millennium - ERROR", MB_ICONERROR);
	}
	return true;
}
