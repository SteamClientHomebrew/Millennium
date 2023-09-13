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

std::string millenniumInstallVersion;
std::string millenniumPatchNotes;

class Handler
{
private:
	static const void listButton(const char* name, int index) {
		static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

		if (current_page == index) {
			//ImGui::Selectable(std::format(" {}", name).c_str());
			ImGui::BulletText(name);
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			ImGui::Selectable(name);
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
			ImGui::PopStyleColor(1);
		}
	};
public:
	__declspec(noinline) static void tos_agreement_page(void)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		if (ImGui::BeginChild("##agree", ImVec2(Region.x, Region.y - 30), true)) {
			ImGui::TextWrapped(terms_and_conditions.c_str());
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::Checkbox("I accept the following agreement", &accepted_tos);
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Continue >").x - 3); // Align cursor to the right

		const char* header_text = " Millennium - Terms of Service";
		const char* httpErrorModal = " Oops!";

		if (ImGui::Button("Continue >")) {
			if (!accepted_tos) {
				ImGui::OpenPopup(header_text);
			}
			else
			{
				try
				{
					const auto info = nlohmann::json::parse(http::get(repo_url));

					millenniumInstallVersion = info["tag_name"];
					millenniumPatchNotes = info["body"];
					current_page = 2;
				}
				catch (const http_error& error) {
					ImGui::OpenPopup(httpErrorModal);
				}
			}
		}

		Window::center_modal(ImVec2(325, 200));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
		if (ImGui::BeginPopupModal(header_text, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			ImGui::TextWrapped("You need to agree with the Terms of Service in order to install Millennium. If you don't agree with the terms please refrain from using our products.");

			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 5);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetStyle().ItemSpacing.y - 23);

			if (ImGui::Button("Close")) {
				ImGui::CloseCurrentPopup();
				show_accept_tos_popup = false;
			}
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		Window::center_modal(ImVec2(325, 200));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
		if (ImGui::BeginPopupModal(httpErrorModal, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			ImGui::TextWrapped("There was an error getting build information on Millennium. Check if you have a valid internet connection"
			" or retry again later. If the problem persists, get help in the discord server.");


			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 85);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetStyle().ItemSpacing.y - 23);

			if (ImGui::Button("need help?")) {
				ShellExecute(0, "open", "https://discord.gg/MXMWEQKgJF", 0, 0, SW_SHOWNORMAL);
			}
			ImGui::SameLine();
			ui::shift::x(-4);
			if (ImGui::Button("Close")) {
				ImGui::CloseCurrentPopup();
				show_accept_tos_popup = false;
			}
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
	}

	__declspec(noinline) static void select_option_page(void)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10, 0.10, 0.10, 1.0f));
		if (ImGui::BeginChild("##install_page", ImVec2(Region.x, Region.y - 30), false))
		{
			static const char* install = "Install Millennium", * reset = "Reset Millennium", * uninstall = "Uninstall Millennium";
			ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

			ImGui::Text("Choose an option:");

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

			ImGui::BeginChild("InstallDetailsPane", ImVec2(rx, 110 + ImGui::CalcTextSize(millenniumPatchNotes.c_str()).y));
			{
				if (selected_option == 1) {
					//ImGui::PushStyleColor(ImGuiCol_Button, col);
					//ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
					if (ImGui::Button(std::format(">> {} <<", install).c_str(), ImVec2(Region.x, 45))) selected_option = 1;
					if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					//ImGui::PopStyleColor(2);
				}
				else {
					if (ImGui::Button(install, ImVec2(Region.x, 45))) selected_option = 1;
					if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				}

				ui::shift::x(10);
				ImGui::BeginChild("BuildInfo", ImVec2(rx, ry), false);
				{
					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
					{
						static const auto path = Steam::getInstallPath();

						ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f),
							std::format("Steam Install: {}\nInstalling Millennium Version: v{}\n\nPatch Notes:\n{}",
								path, millenniumInstallVersion, millenniumPatchNotes
							).c_str());
					}
					ImGui::PopFont();
				}
				ImGui::EndChild();
			}
			ImGui::PopStyleColor(2);

			ImGui::EndChild();
			if (selected_option == 2) {
				//ImGui::PushStyleColor(ImGuiCol_Button, col);
				//ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
				if (ImGui::Button(std::format(">> {} <<", reset).c_str(), ImVec2(Region.x, 45))) selected_option = 2;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				//ImGui::PopStyleColor(2);
			}
			else {
				if (ImGui::Button(reset, ImVec2(Region.x, 45))) selected_option = 2;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			if (selected_option == 3)
			{
				//ImGui::PushStyleColor(ImGuiCol_Button, col);
				//ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
				if (ImGui::Button(std::format(">> {} <<", uninstall).c_str(), ImVec2(Region.x, 45))) selected_option = 3;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				//ImGui::PopStyleColor(2);
			}
			else {
				if (ImGui::Button(uninstall, ImVec2(Region.x, 45))) selected_option = 3;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		std::string buttonName = selected_option == 1 ? "Install >" : selected_option == 2 ? "Reset >" : selected_option == 3 ? "Uninstall >" : "";

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - (selected_option == 0 ? -13 :ImGui::CalcTextSize(buttonName.c_str()).x) - 53); // Align cursor to the right
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));

		if (ImGui::Button("< Back")) current_page = 1;
		ImGui::SameLine(0);

		if (selected_option != 0 && ImGui::Button(buttonName.c_str()))
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

				ImGui::OpenPopup(" Reset Warning");
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
		if (ImGui::BeginChild("##console", ImVec2(Region.x, Region.y - 30), true)) 
		{	
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[3]);
			{
				ImGui::TextWrapped(standard_out.str().c_str());
			}
			ImGui::PopFont();
		}
		ImGui::EndChild();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));

		if (ImGui::Button("Discord")) {
			ShellExecuteA(nullptr, "open", "https://discord.gg/MXMWEQKgJF", nullptr, nullptr, SW_SHOWNORMAL);
		}
		ImGui::SameLine();
		if (ImGui::Button("Github")) {
			ShellExecuteA(nullptr, "open", "https://github.com/ShadowMonster99/millennium-steam-patcher", nullptr, nullptr, SW_SHOWNORMAL);
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close >").x - 60); // Align cursor to the right

		if (ImGui::Button("< Back")) current_page = 2;
		ImGui::SameLine();
		if (ImGui::Button("Close >"))
		{
			ShellExecuteA(nullptr, "open", "https://discord.gg/MXMWEQKgJF", nullptr, nullptr, SW_SHOWNORMAL);
			exit(1);
		}

		ImGui::PopStyleVar();
	}

	static void renderContentPanel()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
		{
			if (ImGui::BeginChild("ChildContentPane", ImVec2(rx, ry), true))
			{
				ImGui::BeginChild("HeaderDrag", ImVec2(rx, 15), false);
				{
					if (ImGui::IsWindowHovered()) {
						g_headerHovered = true;
					}
					else {
						g_headerHovered = false;
					}
				}
				ImGui::EndChild();

				switch (current_page)
				{
					case 1: tos_agreement_page(); break;
					case 2: select_option_page(); break;
					case 3: console_page(); break;
				}
			}
			ImGui::EndChild();
		}
		ImGui::PopStyleColor();
	}

	static void renderSideBar()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.00f));

		ImGui::BeginChild("LeftSideBar", ImVec2(170, ry), false);
		{
			ImGui::PopStyleVar();
			ImGui::BeginChild("ChildFrameParent", ImVec2(rx, ry), true);
			{
				ImGui::BeginChild("HeaderOptions", ImVec2(rx, 25));
				{
					if (ImGui::IsWindowHovered())
					{
						ImGui::Image(Window::iconsObj().close, ImVec2(11, 11));
						if (ImGui::IsItemClicked()) {
							g_windowOpen = false;
							PostQuitMessage(0);
						}

						ImGui::SameLine();
						ui::shift::x(-6);

						ImGui::Image(Window::iconsObj().maximize, ImVec2(11, 11));
						if (ImGui::IsItemClicked()) {
							ShowWindow(Window::getHWND(), SW_SHOWMAXIMIZED);
						}

						ImGui::SameLine();
						ui::shift::x(-6);

						ImGui::Image(Window::iconsObj().minimize, ImVec2(11, 11));
						if (ImGui::IsItemClicked()) {
							ShowWindow(Window::getHWND(), SW_MINIMIZE);
						}
					}
					else
					{
						ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));

						ImGui::SameLine();
						ui::shift::x(-6);
						ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));

						ImGui::SameLine();
						ui::shift::x(-6);
						ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));
					}

					ImGui::SameLine();

					ImGui::BeginChild("HeaderDragArea", ImVec2(rx, ry));
					{
						if (ImGui::IsWindowHovered()) {
							g_headerHovered_1 = true;
						}
						else {
							g_headerHovered_1 = false;
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();

				ImGui::Spacing();
				ImGui::Spacing(); 
				ImGui::Spacing();

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				{
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "TERMS/CONDITIONS");
				}
				ImGui::PopFont();

				listButton(" Terms of Service", 1);
				listButton(" License Agreement", 2);

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				{
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "INSTALLER");
				}
				ImGui::PopFont();

				listButton(" Install Millennium", 3);

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				{
					std::string buildInfo = std::format("Build Date: {}", __DATE__);

					ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - ImGui::CalcTextSize(buildInfo.c_str()).y);

					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), buildInfo.c_str());
				}
				ImGui::PopFont();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
	}

	__declspec(noinline) static void Render(void)
	{
		auto center = [](float avail_width, float element_width, float padding = 0) { ImGui::SameLine((avail_width / 2) - (element_width / 2) + padding); };
		auto center_text = [&](const char* format, float spacing = 15, ImColor color = ImColor(255, 255, 255)) { center(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(format).x); ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);	ImGui::TextColored(color.Value, format); };

		Handler::renderSideBar();
		ImGui::SameLine(); ui::shift::x(-10);
		Handler::renderContentPanel();

		Window::center_modal(ImVec2(200, 125));
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


		Window::center_modal(ImVec2(215, 150));
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	try {
		//redirect std::cout events
		std::cout.rdbuf(standard_out.rdbuf());

		try {
			terms_and_conditions = http::get("https://raw.githubusercontent.com/ShadowMonster99/millennium-steam-patcher/main/.terms-of-service");
		}
		catch (const http_error& error) {
			terms_and_conditions =
				"Couldn't GET Millennium's TOS. Check if you have a valid internet connection or try again later. If problem persists, get help in our discord server.\n"
				"Do not proceed with the installation if you haven't read the TOS";
		}
		
		Window::setTitle((char*)"Millennium Installer");
		Window::setDimensions(ImVec2({ 450, 300 }));

		Application::Create(&Handler::Render, hInstance);
	}
	catch (std::exception& ex) {
		MessageBoxA(0, ex.what(), "Millennium - ERROR", MB_ICONERROR);
	}
	return true;
}
