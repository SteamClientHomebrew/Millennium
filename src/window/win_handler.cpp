#define _WINSOCKAPI_  
#include <stdafx.h>
#include <core/injector/event_handler.hpp>

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

#include <core/injector/conditions/conditionals.hpp>

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

	nlohmann::basic_json<> g_conditionals;

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

		g_conditionals = conditionals::get_conditionals(skin["native-name"]);

		m_editObj = skin;
		comboBoxItems.clear();
		m_editMenuOpen = true;

		//editor::create();
	}

	void deleteListing(std::string fileName) {
#ifdef _WIN32

		//MsgBox("Confirmation", [&](auto open) {
		//	ImGui::TextWrapped(fmt::format("Are you sure you want to delete {}?\nThis cannot be undone.", fileName).c_str());


		//	if (ImGui::Button("Yes")) 
		//	{
		//		std::string disk_path = fmt::format("{}/{}", config.getSkinDir(), fileName);
		//		if (std::filesystem::exists(disk_path)) {

		//			try {
		//				std::filesystem::remove_all(std::filesystem::path(disk_path));
		//			}
		//			catch (const std::exception& ex) {
		//				//MsgBox(fmt::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
		//				console.err(fmt::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str());
		//			}
		//		}
		//		m_Client.parseSkinData(false);
		//	}
		//	if (ImGui::Button("No")) {
		//		*open = false;
		//	}
		//});


		//int result = MsgBox(fmt::format("Are you sure you want to delete {}?\nThis cannot be undone.", fileName).c_str(), "Confirmation", MB_YESNO | MB_ICONINFORMATION);
		//if (result == IDYES)
		//{
		//	std::string disk_path = fmt::format("{}/{}", config.getSkinDir(), fileName);
		//	if (std::filesystem::exists(disk_path)) {

		//		try {
		//			std::filesystem::remove_all(std::filesystem::path(disk_path));
		//		}
		//		catch (const std::exception& ex) {
		//			MsgBox(fmt::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
		//		}
		//	}
		//	m_Client.parseSkinData(false);
		//}
#elif __linux__
        console.err("deleteListing HAS NO IMPLEMENTATION");
#endif
	}

	void library_panel()
	{
		static float contentHeight = 100.0f; // Initialize with a minimum height

		if (g_fileDropQueried) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		}

		ImGui::BeginChild("###library_container", ImVec2(rx, contentHeight), true);
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
				for (int i = 0; i < (int)m_Client.skinData.size(); i++) 
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

						std::string version = fmt::format("version: {}", m_Client.skinData[i].value("version", "1.0.0"));
						std::string author = fmt::format("author: {}", m_Client.skinData[i].value("author", "anonymous"));

						ImGui::SetNextWindowPos(ImVec2(pos.x + width, pos.y + height));
						
#ifdef _WIN32
						ImGui::SetNextWindowSize(ImVec2(max(ImGui::CalcTextSize(version.c_str()).x, ImGui::CalcTextSize(author.c_str()).x) + 25.0f, 0));
#elif __linux__
						ImGui::SetNextWindowSize(ImVec2(std::max(ImGui::CalcTextSize(version.c_str()).x, ImGui::CalcTextSize(author.c_str()).x) + 25.0f, 0));
#endif

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
								auto path = fmt::format("{}/{}", config.getSkinDir(), contextName);
								OpenURL(path.c_str());
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

				//if (steam::get().params.has("-silent")) {
				//	MsgBox("Steam is launched in -silent mode so you need to open steam again from the task tray for it to re-open", "Millennium", MB_ICONINFORMATION);
				//}

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
					std::string loadUrl = fmt::format("SteamUIStore.Navigate('/browser', MainWindowBrowserManager.LoadURL('{}'));", url);

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

			ui::shift::y(3);

#ifdef _WIN32

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

			float width = ImGui::CalcTextSize(corner_prefs[corner_pref]).x + 50;
			ui::shift::right((int)width); ui::shift::y(-3);

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


			ImGui::Text("Acrylic Drop Shadow");

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
					ImGui::Text("Use Acrylic effect on all applicable Steam windows");
				}
				ImGui::EndTooltip();
			}

			ImGui::SameLine();

			ui::shift::right(25); ui::shift::y(-3);
			if (ImGui::Checkbox("###Mica Drop Shadow", &applyMica)) {
				Settings::Set("mica-effect", applyMica);
				SteamJSContext.reload();
			}

			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

			ImGui::Spacing(); 
			ImGui::Separator();
			ImGui::Spacing();
#endif

#ifdef __linux__
            static std::string accent_col = Settings::Get<std::string>("accent-col");

            ImGui::Text("Linux Accent Color");

            ImGui::SameLine();

            ui::shift::x(-3);
            ImGui::TextWrapped("(?)");

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Custom set a accent color for steam windows on Linux");
            }

            ImGui::SameLine();

            static ImVec4 color = colors::HexToImVec4(accent_col);

            //ImGui::ColorPicker3("###AccentCol", &color);
            ImGui::PushItemWidth(90);
            ImGui::ColorEdit3("###AccentCol", &color.x, ImGuiColorEditFlags_DisplayHex);
            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::Button("Save Color")) {
                Settings::Set<std::string>("accent-col", "#" + colors::ImVec4ToHex(color));
                SteamJSContext.reload();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
#endif

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
					Settings::Set("NotificationsPos", nlohmann::json::parse(SteamJSContext.exec_command(fmt::format("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {}", notificationPos)))["result"]["value"].get<int>());
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
					ui::shift::y((int)((ry / 2) - 20));
					ui::center_text("Drop theme here to install...");
				}
				else if (g_processingFileDrop) {

					static float contentHeight = 100.0f; // Initialize with a minimum height

					if (g_fileDropQueried) {
						ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
					}

					ImGui::BeginChild("###library_container", ImVec2(rx, contentHeight), true);
					{
						ui::shift::y(-8);
						ImGui::Text("One Moment...");
						ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), g_fileDropStatus.c_str());
						ui::shift::y(6);
						ImGui::Spinner("##spinner", 10, 2, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

						contentHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
					}
					ImGui::EndChild();

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

		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);

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
				if (ImGui::MenuItem("Themes")) 
				{
					OpenURL(config.getSkinDir().c_str());
				}
				if (ImGui::IsItemHovered())
				{
					static std::string skinDir = config.getSkinDir().c_str();

					ImGui::SetTooltip(fmt::format("Open the themes folder\n{}", skinDir).c_str());
				}
				if (ImGui::MenuItem("Settings"))
				{
					showingSettings = !showingSettings;
				}
				if (ImGui::MenuItem("User-Config"))
				{
					std::string path = std::filesystem::current_path().string() + "/.millennium/config/";
					std::cout << path << std::endl;

					OpenURL(path.c_str());
				}
				if (ImGui::MenuItem("Logs"))
				{
					std::string path = std::filesystem::current_path().string() + "/.millennium/logs/";
					std::cout << path << std::endl;

					OpenURL(path.c_str());
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				bool enabled = skin_json_config.contains("Conditions");

				if (ImGui::MenuItem("Edit Theme", 0, false, enabled))
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
					OpenURL("https://millennium.web.app/discord");
				}
				if (ImGui::IsItemHovered()) 
				{
					ImGui::SetTooltip("Join the discord server...");
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Donate")) 
			{
				if (ImGui::MenuItem("Support Millennium!")) 
				{
					OpenURL("https://ko-fi.com/shadowmonster");
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
			ImGui::PopStyleColor();
		}

		if (editingTheme) 
		{
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(450, 350), ImGuiCond_Once);

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
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(382, 290), ImGuiCond_Once);

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


			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(255, 160));

			ImGui::Begin("About Millennium", &showingAbout, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			{
				std::string version = fmt::format("Version: {}", m_ver);
				std::string built = fmt::format("Built: {}", __DATE__);

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

		ImGui::PopStyleVar();
	}
} RendererProc;

void render_conditionals()
{
	enum type
	{
		checkbox,
		combobox
	};

	for (auto& [key, condition] : RendererProc.m_editObj["Conditions"].items()) {

		if (!condition.contains("values")) {
			continue;
		}

		const std::string description = condition.value("description", "Nothing here...");

		const auto get_type = ([&](void) -> type 
		{
			bool is_bool = condition["values"].contains("yes") && condition["values"].contains("no");
			return is_bool ? checkbox : combobox;
		});

		const std::string theme_name = RendererProc.m_editObj["native-name"];

		switch (get_type())
		{
			case checkbox:
			{
				const std::string str_val = RendererProc.g_conditionals[key];

				bool value = str_val == "yes" ? true : false;

				ui::render_setting(
					key.c_str(), description.c_str(), value, false, [&]() 
					{
						const auto new_val = value ? "yes" : "no";

						RendererProc.g_conditionals[key] = new_val;
						if (!conditionals::update(theme_name, key, new_val))
						{
							std::cout << "failed to update config setting" << std::endl;
						}
					}
				);
				ImGui::Spacing();
				break;
			}
			case combobox:
			{
				const std::string value = RendererProc.g_conditionals[key];
				const int combo_size = condition["values"].size();

				if (!comboAlreadyExists(key))
				{
					int out = -1;
					int i = 0;
					for (auto& [_val, options] : condition["values"].items())
					{
						if (_val == value) { out = i; }
						i++;
					}
					comboBoxItems.push_back({ key, out });
				}

				std::vector<std::string> items;

				for (auto& el : condition["values"].items()) 
				{
					items.push_back(el.key());
				}

				ImGui::Text(key.c_str());
				ImGui::SameLine();

				ui::shift::x(-3);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::TextWrapped("(?)");
				ImGui::PopStyleColor();

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip(description.c_str());
				}

				ImGui::SameLine();

				ui::shift::right(120);
				ImGui::PushItemWidth(120);
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.15f, .15f, .15f, 1.f));

				if (ImGui::BeginCombo(fmt::format("###{}", key).c_str(), items[getComboValue(key)].c_str())) 
				{
					for (int i = 0; i < (int)items.size(); i++) 
					{
						const bool isSelected = (getComboValue(key) == i);
						if (ImGui::Selectable(items[i].c_str(), isSelected))
						{
							setComboValue(key, i);
							conditionals::update(theme_name, key, items[i]);
						}
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::PopStyleColor();
				ImGui::PopItemWidth();
				ImGui::Spacing();

				break;
			}
		}


	}
}

void handleEdit()
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::BeginChild("###ConfigContainer", ImVec2(rx, ry - 27), false);
	{
		ui::shift::y(5);
		render_conditionals();
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ui::shift::right(75);

	if (ImGui::Button("Save", ImVec2(75, 0)))
	{
		m_Client.parseSkinData(false);
		skin_json_config = config.getThemeData();

		themeConfig::updateEvents::getInstance().triggerUpdate();

		steam_js_context js_context;
		js_context.reload();
		return;
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
	Window::setDimensions(ImVec2({ 305, 145 }));

	Application::Create(wndProcCallback, initCallback);
}
