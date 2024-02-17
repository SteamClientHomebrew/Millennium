#define _WINSOCKAPI_  
#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <d3dx9tex.h>
#include <d3dx9.h>

#include <window/core/window.hpp>
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

#include <window/interface_v2/editor.hpp>
#include <core/steam/window/manager.hpp>

bool updateLibrary = false;
void handleEdit();

struct render
{
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

	void deleteListing(std::string fileName) {
		int result = MessageBoxA(GetForegroundWindow(), std::format("Are you sure you want to delete {}?\nThis cannot be undone.", fileName).c_str(), "Confirmation", MB_YESNO | MB_ICONINFORMATION);
		if (result == IDYES)
		{
			std::string disk_path = std::format("{}/{}", config.getSkinDir(), fileName);
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

	void library_panel()
	{
		static float contentHeight = 100.0f; // Initialize with a minimum height

		if (g_fileDropQueried) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		}

		ImGui::BeginChild("###library_container", ImVec2(rx, contentHeight), true, ImGuiWindowFlags_AlwaysAutoResize);
		{
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			std::string currentTheme = m_Client.skinData.size() == 0 ? m_Client.skinDataReady ? "Nothing here..." : "One Moment..." : "< default >";
			for (size_t i = 0; i < m_Client.skinData.size(); ++i)
			{
				nlohmann::basic_json<>& skin = m_Client.skinData[i];
				if (skin.value("native-name", std::string()) == m_Client.m_currentSkin)
				{
					currentTheme = skin.value("name", "null");
				}
			}

			ImGui::Text("Select a theme:");
			ImGui::PushItemWidth(225);

			if (ImGui::BeginCombo("###SelectTheme", currentTheme.c_str())) 
			{
				for (int i = 0; i < m_Client.skinData.size(); i++) 
				{
					static std::string contextName;

					bool contextMenuIsOpen = contextName == m_Client.skinData[i].value("native-name", "");

					// dont display the active skin
					if (m_Client.skinData[i].value("native-name", "null") == m_Client.m_currentSkin) {
						continue;
					}

					// get cursor height before rendering selectable
					const auto height = ImGui::GetCursorPosY() - ImGui::GetScrollY();

					if (ImGui::Selectable(m_Client.skinData[i].value("name", "null").c_str()))
					{
						m_Client.changeSkin((nlohmann::basic_json<>&)m_Client.skinData[i]);
					}
					if (ImGui::IsItemHovered()) {
						
						const auto pos = ImGui::GetWindowPos();
						const auto width = ImGui::GetWindowWidth();

						std::string version = std::format("version: {}", m_Client.skinData[i].value("version", "1.0.0"));
						std::string author = std::format("author: {}", m_Client.skinData[i].value("author", "anonymous"));

						ImGui::SetNextWindowPos(ImVec2(pos.x + width, pos.y + height));
						ImGui::SetNextWindowSize(ImVec2(max(ImGui::CalcTextSize(version.c_str()).x, ImGui::CalcTextSize(author.c_str()).x) + 25.0f, 0));

						ImGui::BeginTooltip();
						{
							ImGui::Text(version.c_str());
							ImGui::TextWrapped(author.c_str());
						}
						ImGui::EndTooltip();
					}


					if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
					{
						contextName = m_Client.skinData[i]["native-name"];
					}

					if (contextMenuIsOpen)
					{
						if (ImGui::BeginPopupContextWindow()) 
						{
							ImGui::Text(m_Client.skinData[i].value("name", "null").c_str());
							ImGui::Separator();
							if (ImGui::MenuItem("Show in folder...")) 
							{
								auto path = std::format("{}/{}", config.getSkinDir(), contextName);
								OpenURL(path.c_str())
							}
							if (ImGui::MenuItem("Delete...")) {
								this->deleteListing(contextName);
							}
							ImGui::EndPopup();
						}
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ui::shift::x(-4);

			if (ImGui::Button("Reset")) {

				Settings::Set("active-skin", "default");
				themeConfig::updateEvents::getInstance().triggerUpdate();

				steam_js_context js_context;
				js_context.reload();

				if (steam::get().params.has("-silent")) {
					MsgBox("Steam is launched in -silent mode so you need to open steam again from the task tray for it to re-open", "Millennium", MB_ICONINFORMATION);
				}

				m_Client.parseSkinData(false);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::SetTooltip("Go back to default.");
				ImGui::PopStyleColor();
			}
			if (g_fileDropQueried) {
				ImGui::PopStyleColor();
			}
			ImGui::PopItemWidth();

			ImGui::TextColored(ImVec4(0.451f, 0.569f, 1.0f, 1.0f), "Get more themes ->");

			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				ImGui::SetTooltip("opens in the main Steam browser...");
				ImGui::PopStyleColor();
			}

			if (ImGui::IsItemClicked()) {
				std::thread([] {
					steam_js_context SharedJsContext;

					std::string url = "https://millennium.web.app/themes";
					std::string loadUrl = std::format("SteamUIStore.Navigate('/browser', MainWindowBrowserManager.LoadURL('{}'));", url);

					SharedJsContext.exec_command(loadUrl);
				}).detach();
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
			static const char* corner_prefs[] = { "Default"/*0*/, "Force Rounded"/*0*/, "Square"/*1*/, "Slightly Rounded"/*2*/ };
			static int corner_pref = Settings::Get<int>("corner-preference");

			static bool applyMica = Settings::Get<bool>("mica-effect");

			ImGui::Text("Window Borders");
			ImGui::SameLine();
			ui::shift::x(-3);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.0f));
			ImGui::TextWrapped("(?)");
			ImGui::PopStyleColor();

			if (ImGui::IsItemHovered()) {

				ImGui::BeginTooltip();
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.0f));
					ImGui::TextWrapped("WINDOWS 11 ONLY, WIP RESULTS MAY VARY");
					ImGui::PopStyleColor();
					ImGui::Text("Custom border radius options for the Steam windows");
				}
				ImGui::EndTooltip();
			}

			ImGui::SameLine();

			int width = ImGui::CalcTextSize(corner_prefs[corner_pref]).x + 50;
			ui::shift::right(width); ui::shift::y(-3);

			ImGui::PushItemWidth(width);
			if (ImGui::Combo("###{}Window Borders", &corner_pref, corner_prefs, IM_ARRAYSIZE(corner_prefs))) {
				Settings::Set("corner-preference", corner_pref);

				updateHook();

				if (corner_pref == 0) {
					SteamJSContext.reload();
				}
			}
			ImGui::PopItemWidth();
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			ImGui::Spacing();


			ImGui::Text("Mica Drop Shadow");

			ImGui::SameLine();

			ui::shift::x(-3);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.0f));
			ImGui::TextWrapped("(?)");
			ImGui::PopStyleColor();

			if (ImGui::IsItemHovered()) {

				ImGui::BeginTooltip();
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.0f));
					ImGui::TextWrapped("WINDOWS 11 ONLY, WIP RESULTS MAY VARY");
					ImGui::PopStyleColor();
					ImGui::Text("Use Mica effect on all applicable Steam windows");
				}
				ImGui::EndTooltip();
			}

			ImGui::SameLine();

			ui::shift::right(15); ui::shift::y(-3);
			if (ImGui::Checkbox("###Mica Drop Shadow", &applyMica)) {
				Settings::Set("mica-effect", applyMica);
				SteamJSContext.reload();
			}

			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			ImGui::Spacing(); 
			ImGui::Separator();
			ImGui::Spacing();

			static bool enable_store = Settings::Get<bool>("auto-update-themes");
			static bool enable_notifs = Settings::Get<bool>("auto-update-themes-notifs");

			ui::render_setting(
				"Auto Update Themes", "Controls whether skins will automatically update when steam starts.",
				enable_store, false,
				[=]() {
					Settings::Set("auto-update-themes", enable_store);
				}
			);
			ImGui::Spacing();

			ui::render_setting(
				"Auto Update Notifs", "Auto Update will still function, just no notifications.",
				enable_notifs, false,
				[=]() {
					Settings::Set("auto-update-themes-notifs", enable_notifs);
				}
			);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			static bool enable_css = Settings::Get<bool>("allow-stylesheet");
			ui::render_setting(
				"StyleSheet Insertion", "Allow CSS (StyleSheet) insertions in the skins you use. CSS is always safe.",
				enable_css, false,
				[=]() {
					Settings::Set("allow-stylesheet", enable_css);
					SteamJSContext.reload();
				}
			);

			ImGui::Spacing();

			static bool enable_js = Settings::Get<bool>("allow-javascript");
			ui::render_setting(
				"JavaScript Execution", "Allow JavaScript executions in the skins you use.\nOnly allow JS execution on skins of authors you trust.",
				enable_js, false,
				[=]() {
					Settings::Set("allow-javascript", enable_js);
					SteamJSContext.reload();
				}
			);
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			static const char* items[] = { "Top Left"/*0*/, "Top Right"/*1*/, "Bottom Left"/*2*/, "Bottom Right"/*3*/ };
			static int notificationPos = nlohmann::json::parse(SteamJSContext.exec_command("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position"))["result"]["value"];

			ui::render_setting_list(
				"Notifications Position", "Adjusts the position of the client notifications instead of using its native coordinates.",
				notificationPos, items, IM_ARRAYSIZE(items), false,
				[=]() {
					Settings::Set("NotificationsPos", nlohmann::json::parse(SteamJSContext.exec_command(std::format("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {}", notificationPos)))["result"]["value"].get<int>());
				}
			);
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
				if (g_fileDropQueried) {
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
		static bool showingAbout = false, showingSettings = false, editingTheme = false;

		if (ImGui::BeginMenuBar()) {

			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));

			if (ImGui::BeginMenu("File")) 
			{
				if (ImGui::MenuItem("Reload")) 
				{
					m_Client.parseSkinData(false);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Check for new themes added");
				}
				if (ImGui::MenuItem("Peek Themes...")) 
				{
					ShellExecuteA(NULL, "open", config.getSkinDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
				}
				if (ImGui::IsItemHovered())
				{
					static std::string skinDir = config.getSkinDir().c_str();

					ImGui::SetTooltip(std::format("Open the themes folder\n{}", skinDir).c_str());
				}
				if (ImGui::MenuItem("Settings"))
				{
					showingSettings = !showingSettings;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Edit Theme")) 
				{
					this->openPopupMenu(skin_json_config);
					editingTheme = !editingTheme;
				}
				if (ImGui::MenuItem("Delete Theme")) 
				{
					this->deleteListing(m_Client.m_currentSkin);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) 
			{
				if (ImGui::MenuItem("About")) 
				{
					showingAbout = !showingAbout;
				}
				if (ImGui::MenuItem("Discord")) 
				{
					ShellExecute(NULL, "open", "https://millennium.web.app/discord", NULL, NULL, SW_SHOWNORMAL);
				}
				if (ImGui::IsItemHovered()) 
				{
					ImGui::SetTooltip("Join the discord server...");
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Donate")) 
			{
				if (ImGui::MenuItem("Help support Millennium!")) 
				{
					ShellExecute(NULL, "open", "https://ko-fi.com/shadowmonster", NULL, NULL, SW_SHOWNORMAL);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
			ImGui::PopStyleColor();
		}

		if (editingTheme) 
		{
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(325, 250), ImGuiCond_Once);

			ImGui::Begin("Editing a theme", &editingTheme, ImGuiWindowFlags_NoCollapse);
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
				handleEdit();
				ImGui::PopStyleColor();
			}
			ImGui::End();
			ImGui::PopStyleColor();
		}
		if (showingSettings) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(382, 288), ImGuiCond_Once);

			ImGui::Begin("Settings", &showingSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
				settings_panel();
				ImGui::PopStyleColor();
			}
			ImGui::End();
			ImGui::PopStyleColor();
		}
		if (showingAbout) {

			ImGuiWindowClass window_class1;
			window_class1.ViewportFlagsOverrideSet = ImGuiViewportFlags_IsPlatformWindow;
			//window_class1.ViewportFlagsOverrideClear = ImGuiViewportFlags_IsPlatformWindow;

			ImGui::SetNextWindowClass(&window_class1);


			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(255, 160));

			ImGui::Begin("About Millennium", &showingAbout, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			{
				std::string version = std::format("Version: {}", m_ver);
				std::string built = std::format("Built: {}", __DATE__);

				ImGui::Text("Made with love by ShadowMonster.");
				ImGui::Text(version.c_str());
				ImGui::Text(built.c_str());

				ImGui::TextColored(ImVec4(0.451f, 0.569f, 1.0f, 1.0f), "View source code");

				if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					ImGui::SetTooltip("https://github.com/ShadowMonster99/millennium-steam-patcher/");
				}
				if (ImGui::IsItemClicked()) {
					OpenURL("https://github.com/ShadowMonster99/millennium-steam-patcher/");
				}
				ImGui::Text("Don't forget to star the project :)");
			}


			ImGui::End();
			ImGui::PopStyleColor();
		}
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
						name.c_str(), toolTip.c_str(), value, false, [&]() { setting["Value"] = value; }
					);
					ImGui::Spacing();
				}
				else if (type == "ComboBox") 
				{
					std::string value = setting.value("Value", std::string());

					if (!comboAlreadyExists(name)) 
					{
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

					ui::shift::x(-3);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
					ImGui::TextWrapped("(?)");
					ImGui::PopStyleColor();

					if (ImGui::IsItemHovered()) 
					{
						ImGui::SetTooltip(toolTip.c_str());
					}
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
					ImGui::Spacing();
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
				else 
				{
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
		ImGui::PopStyleVar();

		RendererProc.renderSideBar();
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

	Window::setTitle((char*)" Millennium.");
	Window::setDimensions(ImVec2({ 450, 500 }));

	Application::Create(wndProcCallback, initCallback);
}
