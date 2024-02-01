#define _WINSOCKAPI_  
#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <d3dx9tex.h>
#include <d3dx9.h>

#include <window/core/window.hpp>
#include <window/core/dx9_image.hpp>
#include <window/win_handler.hpp>
#include <window/interface/globals.h>

#include <utils/http/http_client.hpp>
#include <utils/thread/thread_handler.hpp>

#include <window/api/installer.hpp>
#include <window/core/colors.hpp>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <set>

#include <core/steam/cef_manager.hpp>
#include "core/memory.h"

bool updateLibrary = false;
void handleEdit();

struct ComboBoxItem
{
	std::string name;
	int value;
};

std::vector<ComboBoxItem> comboBoxItems;

bool comboAlreadyExists(const std::string& name)
{
	for (const auto& item : comboBoxItems)
	{
		if (item.name == name)
			return true;
	}
	return false;
}	

int getComboValue(const std::string name)
{
	for (const auto& item : comboBoxItems)
	{
		if (item.name == name)
			return item.value;
	}
	return -1;
}

void setComboValue(const std::string name, int value)
{
	for (auto& item : comboBoxItems)
	{
		if (item.name == name)
		{
			item.value = value;
			return;
		}
	}
}	

struct render
{
private:
	nlohmann::basic_json<> m_itemSelectedSource;

	const void listButton(const char* name, int index) {
		static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

		if (ui::current_tab_page == index) {
			if (ImGui::Button(name)) ui::current_tab_page = index;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			if (ImGui::Button(name)) ui::current_tab_page = index;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
			ImGui::PopStyleColor(1);
		}
	};
public:

	nlohmann::json m_editObj;
	bool m_editMenuOpen = false;

	struct ColorInfo {
		std::string name;
		ImVec4 color;
		ImVec4 original;
		std::string comment;
	};
	std::vector<ColorInfo> colorList;

	void openPopupMenu(nlohmann::basic_json<>& skin)
	{
		colorList.clear();

		if (skin.contains("GlobalsColors") && skin.is_object())
		{
			for (const auto& color : skin["GlobalsColors"])
			{
				if (!color.contains("ColorName") || !color.contains("HexColorCode") || !color.contains("OriginalColorCode") || !color.contains("Description"))
				{
					console.err("Couldn't collect global color. 'ColorName' or 'HexColorCode' or 'OriginalColorCode' or 'Description' doesn't exist");
					continue;
				}
				colorList.push_back({
					color["ColorName"],
					colors::HexToImVec4(color["HexColorCode"]),
					colors::HexToImVec4(color["OriginalColorCode"]),
					color["Description"]
				});
			}
		}
		else {
			console.log("Theme doesn't have GlobalColors");
		}
		m_editObj = skin;
		comboBoxItems.clear();
		m_editMenuOpen = true;
	}

	void deleteListing(nlohmann::basic_json<>& skin) {
		int result = MessageBoxA(GetForegroundWindow(), std::format("Are you sure you want to delete {}?\nThis cannot be undone.", skin["native-name"].get<std::string>()).c_str(), "Confirmation", MB_YESNO | MB_ICONINFORMATION);
		if (result == IDYES)
		{
			std::string disk_path = std::format("{}/{}", config.getSkinDir(), skin["native-name"].get<std::string>());
			if (std::filesystem::exists(disk_path)) {

				try {
					std::filesystem::remove_all(std::filesystem::path(disk_path));
				}
				catch (const std::exception& ex) {
					MsgBox(std::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
				}
			}
			m_Client.parseSkinData(false);
		}
	}

	void createLibraryListing(nlohmann::basic_json<> skin, int index, bool deselect = false)
	{
		const std::string m_skinName = skin.value("native-name", std::string());
		static bool push_popped = false;
		static int hovering;

		bool popped = false;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		float desc_height = ImGui::CalcTextSize(skin["description"].get<std::string>().c_str(), 0, false, ImGui::GetContentRegionAvail().x - 50).y;

		if (hovering == index) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			push_popped = true;
		}
		else ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		static bool btn1hover = false;
		static bool btn2hover = false;
		static bool btn3hover = false;

		if (m_Client.m_currentSkin == m_skinName)
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.27f, 0.27f, 0.27f, 1.0f));
		else
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 0.0f));

		bool requiresUpdate = skin.value("update_required", false);

		ImGui::BeginChild(std::format("card_child_container_{}", index).c_str(), ImVec2(rx, 37), true, ImGuiWindowFlags_ChildWindow);
		{
			ImGui::PopStyleVar();
			popped = true;
			//ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6);
			ImGui::PopStyleColor();


			ImGui::BeginChild(std::format("skin_header{}", index).c_str(), ImVec2(rx, 37), true, ImGuiWindowFlags_NoScrollbar);
			{
				//ImGui::Image((void*)Window::iconsObj().skin_icon, ImVec2(ry - 1, ry - 1));
				//ImGui::SameLine();
				//ui::shift::x(-4);
				ImGui::Text(skin.value("name", "null").c_str());
				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - 5, ImGui::GetCursorPosY() + 1));
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), std::format("{} by {}", skin.value("version", "1.0.0"), skin.value("author", "unknown")).c_str());
				ImGui::SameLine();
				ui::shift::right(35);
				ImGui::SameLine();
				ui::shift::y(1);
				ImGui::PopFont();

				if (requiresUpdate)
				{
					if (hovering == index)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_CheckMark));

						ui::shift::y(-3);
						ui::shift::right(159);

						ImGui::ImageButton(Window::iconsObj().editIcon, ImVec2(16, 16));
						btn1hover = ImGui::IsItemHovered();

						if (ImGui::IsItemClicked()) {
							this->openPopupMenu(skin);
						}

						ImGui::SameLine(0);
						ui::shift::x(-4);
						ImGui::PopStyleColor(2);

						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
						ImGui::ImageButton(Window::iconsObj().deleteIcon, ImVec2(16, 16));

						btn2hover = ImGui::IsItemHovered();

						if (ImGui::IsItemClicked()) {
							deleteListing(skin);
						}

						ImGui::PopStyleColor(2);
						ImGui::SameLine();
						ui::shift::x(-4);
					}
					else {
						ui::shift::y(-3);
						ui::shift::right(95);
					}

					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_CheckMark));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_CheckMark));

					static bool updateSkinData = false;

					if (updateSkinData) {
						m_Client.parseSkinData(true, true, skin["native-name"]);
						updateSkinData = false;
					}

					if (ImGui::Button("UPDATE", ImVec2(rx, 0))) {
						std::thread([=] {
							Community::Themes->installUpdate(skin);
							updateSkinData = true;
						}).detach();
					}
					if (ImGui::IsItemHovered()) {
						btn3hover = true;

						std::string message = skin.contains("git") 
							&& skin["git"].contains("message") 
							&& !skin["git"]["message"].is_null() ? 
								skin["git"].value("message", "null") : "null";

						std::string date = skin.contains("git") 
							&& skin["git"].contains("date") 
							&& !skin["git"]["date"].is_null() ? 
								skin["git"].value("date", "null") : "null";

						std::string text = std::format("{} was updated on {}.\n\nReason:\n{}\n\nMiddle click to see more details...", skin.value("native-name", "null"), date, message);

						ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.15f, .15f, .15f, 1.f));
						ImGui::SetTooltip(text.c_str());
						ImGui::PopStyleColor();
					}
					else {
						btn3hover = false;
					}

					if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
						OpenURL((skin.contains("git") && skin["git"].contains("url") && !skin["git"]["url"].is_null() ? skin["git"]["url"].get<std::string>() : "null").c_str());
					}

					ImGui::PopStyleColor(2);
				}
				else if (hovering == index)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_CheckMark));

					ui::shift::y(-3);
					ui::shift::right(55);

					ImGui::ImageButton(Window::iconsObj().editIcon, ImVec2(16, 16));
					btn1hover = ImGui::IsItemHovered();


					if (ImGui::IsItemClicked()) 
						this->openPopupMenu(skin);

					ImGui::SameLine(0);
					ui::shift::x(-4);
					ImGui::PopStyleColor(2);

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
					ImGui::ImageButton(Window::iconsObj().deleteIcon, ImVec2(16, 16));

					btn2hover = ImGui::IsItemHovered();
					
					if (ImGui::IsItemClicked()) this->deleteListing(skin);
					
					ImGui::PopStyleColor(2);
				}
			}
			ImGui::EndChild();
			//ImGui::PopStyleVar();		
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
			std::cout << "calling to change skin..." << std::endl;
			m_Client.changeSkin((nlohmann::basic_json<>&)skin);
		}

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			hovering = index;
			push_popped = false;
		}
		else if (push_popped) hovering = -1;

		ImGui::PopStyleColor();
		ImGui::EndChild();
	}
	void library_panel()
	{
		if (m_Client.m_missingSkins > 0) {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, .5f), std::format("{} missing remote skin(s). couldn't connect to endpoint", m_Client.m_missingSkins).c_str());
			ImGui::SameLine();
		}
		if (m_Client.m_unallowedSkins > 0) {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, .5f), std::format("{} missing remote skin(s). networking not allowed", m_Client.m_unallowedSkins).c_str());
			ImGui::SameLine();
		}

		static char text_buffer[150];

		auto worksize = ImGui::GetMainViewport()->WorkSize;
		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		ui::center(rx, child_width, 8);

		static float contentHeight = 100.0f; // Initialize with a minimum height

		if (g_fileDropQueried) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		}

		ImGui::BeginChild("###library_container", ImVec2(child_width, contentHeight), true, ImGuiWindowFlags_AlwaysAutoResize);
		{
			if (g_fileDropQueried) {
				ImGui::PopStyleColor();
			}

			static const auto to_lwr = [](std::string str) {
				std::string result;
				for (char c : str) {
					result += std::tolower(c);
				}
				return result;
			};

			bool skinSelected = false;

			for (size_t i = 0; i < m_Client.skinData.size(); ++i)
			{
				nlohmann::basic_json<>& skin = m_Client.skinData[i];

				if (skin.value("native-name", std::string()) == m_Client.m_currentSkin)
				{
					skinSelected = true;

					createLibraryListing(skin, i, true);

					ImGui::Spacing();
					ImGui::Spacing();
				}
			}

			//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			//{
			//	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "LIBRARY:");
			//}
			//ImGui::PopFont();
			//ImGui::SameLine();
			ui::shift::right(216);

			if (ImGui::ImageButton(Window::iconsObj().reload_icon, ImVec2(17, 17))) {
				m_Client.parseSkinData(false);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::SetTooltip("Reload Library...");
				ImGui::PopStyleColor();
			}

			ImGui::SameLine(); 
			ui::shift::x(-4);

			if (ImGui::ImageButton(Window::iconsObj().foldericon, ImVec2(17, 17))) {
				ShellExecuteA(NULL, "open", config.getSkinDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::SetTooltip("Open the skins folder...");
				ImGui::PopStyleColor();
			}

			ImGui::SameLine();
			ui::shift::x(-4);

			ImGui::PushItemWidth(150);

			auto position = ImGui::GetCursorPos();
			ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer));
			static const auto after = ImGui::GetCursorPos();

			static bool is_focused = ImGui::IsItemActive() || ImGui::IsItemFocused();

			if (!is_focused && text_buffer[0] == '\0')
			{
				ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::TextUnformatted("Search Library...");
				ImGui::PopStyleColor();
			}

			ImGui::PopItemWidth();
			ui::shift::y(5);

			if (!skinSelected ? m_Client.skinData.empty() : m_Client.skinData.size() - 1 <= 0) {
				ui::shift::y((int)(ry / 2 - 25));

				ui::center(0, 240, 0);
				ImGui::BeginChild("noResultsContainer", ImVec2(240, 135), false);
				{
					ImGui::Image(Window::iconsObj().icon_no_results, ImVec2(240, 110));
					const char* text = "You don't have any themes!";

					ui::center(0, ImGui::CalcTextSize(text).x, 0);
					ImGui::Text(text);
				}
				ImGui::EndChild();
			}
			else {
				try {
					std::sort(m_Client.skinData.begin(), m_Client.skinData.end(), ([&](const nlohmann::json& a, const nlohmann::json& b) 
					{
						bool downloadA = a.value("update_required", false);
						bool downloadB = b.value("update_required", false);

						if (downloadA && !downloadB)       return true;		
						else if (!downloadA && downloadB)  return false;
						else                               return a < b;
						
					}));
				}
				catch (nlohmann::detail::exception&) { }

				for (size_t i = 0; i < m_Client.skinData.size(); ++i)
				{
					nlohmann::basic_json<>& skin = m_Client.skinData[i];

					if (skin.value("native-name", std::string()) != m_Client.m_currentSkin &&
						to_lwr(skin["name"].get<std::string>()).find(to_lwr(text_buffer)) != std::string::npos)
					{
						createLibraryListing(skin, i + (m_Client.skinData.size() + 1));
					}
				}
			}

			contentHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
		}
		ImGui::EndChild();
	}
	void settings_panel()
	{
		static steam_js_context SteamJSContext;

		auto worksize = ImGui::GetMainViewport()->WorkSize;
		float child_width = (float)(worksize.x < 800 ? rx - 20 : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		static float windowHeight = 0;

		ui::center(rx, child_width, -1);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15);
		ImGui::BeginChild("settings_panel", ImVec2(child_width, windowHeight), false);
		{
			static bool enable_store = Settings::Get<bool>("auto-update-themes");
			static bool enable_notifs = Settings::Get<bool>("auto-update-themes-notifs");

			ImGui::Text("Auto Updater"); 
			ImGui::Spacing(); ImGui::Spacing();

			ui::shift::x(25);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::TextWrapped("Auto Update Themes - Controls whether skins will automatically update when steam starts.");
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ui::shift::right(15); ui::shift::y(-3);
			if (ImGui::Checkbox(std::format("###{}", "Auto Update Themes").c_str(), &enable_store)) {
				Settings::Set("auto-update-themes", enable_store);
			}

			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			ui::shift::x(25);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::TextWrapped("Display Notifications - Auto Update will still function, just no notification.");
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ui::shift::right(15); ui::shift::y(-3);
			if (ImGui::Checkbox(std::format("###{}", "Display Notifications").c_str(), &enable_notifs)) {
				Settings::Set("auto-update-themes-notifs", enable_notifs);
			}

			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);



			ImGui::Spacing(); ImGui::Spacing();


			static bool enable_css = Settings::Get<bool>("allow-stylesheet");
			ui::render_setting(
				"StyleSheet Insertion", "Allow CSS (StyleSheet) insertions in the skins you use. CSS is always safe.",
				enable_css, false,
				[=]() {
					Settings::Set("allow-stylesheet", enable_css);
					SteamJSContext.reload();
				}
			);

			ImGui::Spacing(); ImGui::Spacing();

			static bool enable_js = Settings::Get<bool>("allow-javascript");
			ui::render_setting(
				"JavaScript Execution", "Allow JavaScript executions in the skins you use.\nOnly allow JS execution on skins of authors you trust.",
				enable_js, false,
				[=]() {
					Settings::Set("allow-javascript", enable_js);
					SteamJSContext.reload();
				}
			);
			ImGui::Spacing(); ImGui::Spacing();

			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing();

			static const char* items[] = { "Top Left"/*0*/, "Top Right"/*1*/, "Bottom Left"/*2*/, "Bottom Right"/*3*/ };
			static int notificationPos = nlohmann::json::parse(SteamJSContext.exec_command("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position"))["result"]["value"];

			ui::render_setting_list(
				"Client Notifications Position", "Adjusts the position of the client location instead of using its native coordinates.",
				notificationPos, items, IM_ARRAYSIZE(items), false,
				[=]() {
					Settings::Set("NotificationsPos", nlohmann::json::parse(SteamJSContext.exec_command(std::format("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {}", notificationPos)))["result"]["value"].get<int>());
				}
			);
			ImGui::Spacing();
			ImGui::Spacing();

			static bool enableUrlBar = Settings::Get<bool>("enableUrlBar");
			ui::render_setting(
				"Store URL Bar", "Force hide the store url bar that displays current location",
				enableUrlBar, true,
				[=]() {
					Settings::Set("enableUrlBar", enableUrlBar);
					SteamJSContext.reload();
				}
			);
			windowHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	void renderContentPanel()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10);
			ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

			if (ImGui::BeginChild("ChildContentPane", ImVec2(rx, ry), true))
			{
				if (RendererProc.m_editMenuOpen) {
					handleEdit();
				}
				else if (g_fileDropQueried) {
					ui::shift::y((ry / 2) - 20);
					ui::center_text("Drop theme here to install...");
				}
				else if (g_processingFileDrop) {

					const int width = ImGui::CalcTextSize(g_fileDropStatus.c_str()).x;

					ui::shift::y((ry / 2) - 45);
					ui::center(rx, width, 1);

					if (ImGui::BeginChild("DropFileStatus", ImVec2(rx, 35), true)) 
					{
						ImGui::Text(g_fileDropStatus.c_str());
					}
					ImGui::EndChild();
					ui::center(rx, 10, 1);

					ImGui::Spinner("##spinner", 10, 2, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

					if (g_openSuccessPopup) {
						m_Client.parseSkinData(false);
						g_openSuccessPopup = false;
					}
				}
				else
				{
					switch (ui::current_tab_page)
					{
						case 2: library_panel();  break;
						case 3: settings_panel(); break;
					}
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		ImGui::PopStyleColor();
	}

	void renderSideBar()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.00f));

		ImGui::BeginChild("LeftSideBar", ImVec2(rx, 40), false);
		{
			ImGui::PopStyleVar();
			ImGui::BeginChild("ChildFrameParent", ImVec2(rx, ry), true);
			{
				g_headerHovered_1 = ImGui::IsWindowHovered();

				ui::shift::x(5);
				ui::shift::y(3);

				std::string headerText = std::format("Millennium v{}", m_ver);
				const int headerWidth = ImGui::CalcTextSize(headerText.c_str()).x;

				ImGui::Text(headerText.c_str());
				ImGui::SameLine();
				ui::shift::y(-3);
				ui::shift::x(220 - headerWidth);

				listButton(" Library ", 2);
				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;
				ImGui::SameLine();
				ui::shift::x(-4);


				listButton(" Settings ", 3);
				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;

				ImGui::SameLine();
				ui::shift::x(-4);

				static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

				if (ImGui::Button("Community")) {
					steam_js_context SharedJsContext;

					std::string url = "https://millennium.web.app/themes";
					std::string loadUrl = std::format("SteamUIStore.Navigate('/browser', MainWindowBrowserManager.LoadURL('{}'));", url);

					SharedJsContext.exec_command(loadUrl);
				}
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); 
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
					//ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);
					g_headerHovered_1 = false;
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					ImGui::SetTooltip("Opens in Steam browser.");
					ImGui::PopStyleColor();
					//ImGui::PopStyleVar();
				}

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

				ImGui::SameLine();
				ui::shift::x(-4);

				if (ImGui::Button("Help")) {
					ShellExecute(NULL, "open", "https://discord.gg/MXMWEQKgJF", NULL, NULL, SW_SHOWNORMAL);
				}
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered()) 
				{
					ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
					//ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);
					g_headerHovered_1 = false;
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					ImGui::SetTooltip("Join the Discord server...");
					ImGui::PopStyleColor();
					//ImGui::PopStyleVar();
				}

				ImGui::SameLine();

				ui::shift::right(25);

				if (ImGui::ImageButton(Window::iconsObj().xbtn, ImVec2(16, 16))) {
					g_windowOpen = false;
					PostQuitMessage(0);
				}
				if (ImGui::IsItemHovered()) g_headerHovered_1 = false;
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}
} RendererProc;

void handleEdit()
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::BeginChild("###ConfigContainer", ImVec2(rx, ry - 32), false);
	{
		const bool hasConfiguration = RendererProc.m_editObj.contains("Configuration");
		const bool hasColors = !RendererProc.colorList.empty();

		if (!hasConfiguration && !hasColors) {
			ImGui::Text("No Settings Available");
		}

		if (hasConfiguration)
		{
			ImGui::Spacing();
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);

			ImGui::Text(" Configuration Settings");
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PopFont();

			for (auto& setting : RendererProc.m_editObj["Configuration"])
			{
				if (!setting.contains("Name"))
					continue;
				if (!setting.contains("Type"))
					continue;

				const std::string name = setting["Name"];
				const std::string toolTip = setting.value("ToolTip", std::string());
				const std::string type = setting["Type"];

				if (type == "CheckBox") {
					bool value = setting.value("Value", false);

					ui::render_setting(
						name.c_str(), toolTip.c_str(),
						value, false,
						[&]() { setting["Value"] = value; }
					);
					ui::shift::y(15);
				}
				else if (type == "ComboBox") {

					std::string value = setting.value("Value", std::string());

					if (!comboAlreadyExists(name)) {

						int out = -1;

						for (int i = 0; i < setting["Items"].size(); i++) {
							if (setting["Items"][i] == value) {
								out = i;
							}
						}
						comboBoxItems.push_back({ name, out });
					}


					const auto items = setting["Items"].get<std::vector<std::string>>();

					ImGui::Text(name.c_str());

					ImGui::SameLine();
					ui::shift::right(120);
					ImGui::PushItemWidth(120);
					ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.15f, .15f, .15f, 1.f));

					if (ImGui::BeginCombo(std::format("###{}", name).c_str(), items[getComboValue(name)].c_str())) {
						for (int i = 0; i < items.size(); i++) {
							const bool isSelected = (getComboValue(name) == i);

							if (ImGui::Selectable(items[i].c_str(), isSelected))
							{
								setComboValue(name, i);
								std::cout << items[i] << " selected" << std::endl;
								setting["Value"] = items[i];
							}
							if (isSelected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					ImGui::PopStyleColor();
					ImGui::PopItemWidth();

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
					ImGui::Bullet();
					ImGui::SameLine();
					ImGui::TextWrapped(toolTip.c_str());
					ImGui::PopStyleColor();

					ui::shift::y(15);
				}
			}
		}

		if (hasColors)
		{
			ImGui::Spacing();
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
			ImGui::Text(" Color Settings:");
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PopFont();

			if (ImGui::Button("Reset Colors"))
			{
				auto& obj = RendererProc.m_editObj;

				if (obj.contains("GlobalsColors") && obj.is_object())
				{
					for (auto& color : obj["GlobalsColors"])
					{
						if (!color.contains("HexColorCode") || !color.contains("OriginalColorCode"))
						{
							console.err("Couldn't reset colors. 'HexColorCode' or 'OriginalColorCode' doesn't exist");
							continue;
						}
						color["HexColorCode"] = color["OriginalColorCode"];
					}
				}
				else {
					console.log("Theme doesn't have GlobalColors");
				}
				config.setThemeData(obj);
				RendererProc.openPopupMenu(obj);
			}

			auto& colors = RendererProc.colorList;

			for (size_t i = 0; i < RendererProc.colorList.size(); i++)
			{
				if (ImGui::Button("Reset"))
				{
					auto& obj = RendererProc.m_editObj;

					if (obj.contains("GlobalsColors") && obj.is_object())
					{
						auto& global = obj["GlobalsColors"][i];

						console.log(std::format("global colors: {}", global.dump(4)));
						console.log(std::format("color name: {}", colors[i].name));

						if (global["ColorName"] == colors[i].name) {

							console.log(std::format("Color match. Setting color {} from {} to {}",
								global["ColorName"].get<std::string>(),
								global["HexColorCode"].get<std::string>(),
								global["OriginalColorCode"].get<std::string>()));

							global["HexColorCode"] = global["OriginalColorCode"];
						}
						else {
							MsgBox(std::format("Couldn't Set color at index {} because the buffer was mismatching.", i).c_str(), "Error", MB_ICONERROR);
						}
					}
					else {
						console.log("Theme doesn't have GlobalColors");
					}

					config.setThemeData(obj);
					m_Client.parseSkinData(false);

					RendererProc.openPopupMenu(obj);

					themeConfig::updateEvents::getInstance().triggerUpdate();

					steam_js_context SharedJsContext;
					SharedJsContext.reload();
				}

				ImGui::SameLine();
				ui::shift::x(-8);

				ImGui::PushItemWidth(90);
				ImGui::ColorEdit3(std::format("##colorpicker_{}", i).c_str(), &colors[i].color.x, ImGuiColorEditFlags_DisplayHex);
				ImGui::PopItemWidth();

				ImGui::SameLine();
				ui::shift::x(-4);

				ImGui::Text(std::format("{} [{}]", colors[i].comment, colors[i].name).c_str());
			}
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ui::shift::right(100);

	if (ImGui::Button(" Save "))
	{
		const auto json = RendererProc.m_editObj;
		nlohmann::json buffer = config.getThemeData(true);

		if (buffer.contains("config_fail") && buffer["config_fail"]) {
			MsgBox("Unable to save and update config. Millennium couldn't get theme data", "Error", MB_ICONERROR);
		}
		else {

			if (buffer.contains("Configuration") && json.contains("Configuration"))
			{
				buffer["Configuration"] = json["Configuration"];
				config.setThemeData(buffer);

				auto& colors = RendererProc.colorList;
				auto& obj = RendererProc.m_editObj;

				if (obj.contains("GlobalsColors") && obj.is_object())
				{
					for (size_t i = 0; i < obj["GlobalsColors"].size(); i++)
					{
						auto& global = obj["GlobalsColors"][i];

						if (!global.contains("HexColorCode")) {
							console.err("Couldn't reset colors. 'HexColorCode' or 'OriginalColorCode' doesn't exist");
							continue;
						}
						if (global["ColorName"] == colors[i].name) {
							global["HexColorCode"] = "#" + colors::ImVec4ToHex(colors[i].color);
						}
						else {
							console.err(std::format("Color at index {} was a mismatch", i));
						}
					}
				}
				else {
					console.log("Theme doesn't have GlobalColors");
				}
				config.setThemeData(obj);
				m_Client.parseSkinData(false);

				themeConfig::updateEvents::getInstance().triggerUpdate();

				steam_js_context SharedJsContext;
				SharedJsContext.reload();
			}
			else {
				console.log("json buffer or editing object doesn't have a 'Configuration' key");
			}
		}
	}
	ImGui::SameLine();
	ui::shift::x(-4);
	if (ImGui::Button(" Close ")) {
		RendererProc.m_editMenuOpen = false;
	}
}

void init_main_window()
{
	g_windowOpen = true;

	const auto initCallback = ([=](void) -> void {

		m_Client.skinData.clear();

		std::thread([&]() {

			m_Client.parseSkinData(true);
		}).detach();
	});
	const auto wndProcCallback = ([=](void) -> void {

		if (updateLibrary) {
			console.log("sync update library info");
			m_Client.parseSkinData(false, false, "");
			updateLibrary = false;
		}

		RendererProc.renderSideBar();
		ui::shift::y(-8);
		RendererProc.renderContentPanel();
	});

	std::thread([&]() {
		themeConfig::watchPath(config.getSkinDir(), []() {
			try {
				updateLibrary = true;
			}
			catch (std::exception& ex) {
				console.log(ex.what());
			}
		});
	}).detach();

	Window::setTitle((char*)"Millennium.Steam.Client");
	Window::setDimensions(ImVec2({ 450, 500 }));

	Application::Create(wndProcCallback, initCallback);
}
