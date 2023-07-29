#define _WINSOCKAPI_   
#include <stdafx.h>
#include <random>

#include <extern/window/src/window.hpp>
#include <extern/window/win_handler.hpp>
#include <include/config.hpp>

#include <extern/steam/cef_manager.hpp>

#define rx ImGui::GetContentRegionAvail().x
#define ry ImGui::GetContentRegionAvail().y

namespace millennium
{
	bool conflicting_skin;
	bool javascript_enabled;

	std::string current_skin;
	std::string conflict_message;
	std::vector<nlohmann::basic_json<>> skin_data;

	std::string last_synced = "never";
}

namespace ui
{
	static int current_tab_page = 2; //library

	namespace region
	{
		int x()
		{
			return ImGui::GetContentRegionAvail().x;
		}
		int y()
		{
			return ImGui::GetContentRegionAvail().y;
		}
	}

	namespace shift
	{
		void x(int value)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + value);
		}
		void y(int value)
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + value);
		}

		void right(int item_width)
		{
			ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - item_width);
		}
	}

	const void render_setting(const char* name, const char* description, bool& setting, bool requires_restart, std::function<void()> call_back = NULL)
	{
		ImGui::Text(name); ImGui::SameLine();

		if (requires_restart)
			ui::shift::x(-5); ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "(Requires Reload)");

		ImGui::SameLine();

		ui::shift::right(15); ui::shift::y(-3);
		if (ImGui::Checkbox(std::format("###{}", name).c_str(), &setting))
			call_back();

		if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), description);
	};
}

static void get_millennium_info(void)
{
	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	std::stringstream ss;
	ss << std::put_time(&*std::localtime(&time), "%H:%M:%S");
	millennium::last_synced = std::format("{}", ss.str());

	std::vector<nlohmann::basic_json<>> temp_json;
	millennium::current_skin = registry::get_registry("active-skin");
	millennium::javascript_enabled = registry::get_registry("allow-javascript") == "true";

	std::unordered_map<std::string, bool> nameMap;

	for (const auto& entry : std::filesystem::directory_iterator(config.get_steam_skin_path()))
	{
		if (entry.path().filename().string().find(".millennium.json") == std::string::npos)
			continue;

		nlohmann::json data; {
			std::ifstream skin_json_file(entry.path().string()); skin_json_file >> data;
		}
		nlohmann::basic_json<> result = nlohmann::json::parse(steam_interface.discover(data["skin-json"]));

		result["name"] = result.value("name", entry.path().filename().string()).c_str();
		result["native-name"] = entry.path().filename().string();
		result["description"] = result.value("description", "no description yet.").c_str();

		std::cout << result.dump(4) << std::endl;

		// Check if the name already exists in the map
		if (nameMap.find(result["name"]) != nameMap.end())
		{
			millennium::conflicting_skin = true;
			millennium::conflict_message = std::format("conflicting skin name [{}] from skin [{}]",
				result["name"].get<std::string>(),
				entry.path().filename().string()
			);

			continue;
		}

		nameMap[result["name"]] = true;
		temp_json.push_back(result);
	}

	for (const auto& entry : std::filesystem::directory_iterator(config.get_steam_skin_path()))
	{
		if (not entry.is_directory())
			continue;

		std::filesystem::path skin_json_path = entry.path() / "skin.json";

		if (!std::filesystem::exists(skin_json_path))
			continue;

		nlohmann::json data;
		{
			std::ifstream skin_json_file(skin_json_path.string());
			skin_json_file >> data;
		}
		data["name"] = data.value("name", entry.path().filename().string()).c_str();
		data["native-name"] = entry.path().filename().string();
		data["description"] = data.value("description", "no description yet.").c_str();

		// Check if the name already exists in the map
		if (nameMap.find(data["name"]) != nameMap.end())
		{
			millennium::conflicting_skin = true;
			millennium::conflict_message = std::format("conflicting skin name [{}] from skin [{}]",
				data["name"].get<std::string>(),
				entry.path().filename().string()
			);

			continue;
		}

		nameMap[data["name"]] = true;

		temp_json.push_back(data);
	}
	millennium::skin_data = temp_json;

}

static void change_steam_skin(nlohmann::basic_json<>& skin)
{
	std::string queried_skin = skin["native-name"].get<std::string>();
	std::string disk_path = std::format("{}/{}/skin.json", config.get_steam_skin_path(), queried_skin);

	if (millennium::current_skin == skin.value("native-name", "null")) {
		queried_skin = "default";
	}

	//whitelist the default skin
	if (queried_skin == "default" || std::filesystem::exists(disk_path))
	{
		//update registry key holding the active-skin selection
		registry::set_registry("active-skin", queried_skin);
		skin_config::skin_change_events::get_instance().trigger_change();

		steam_js_context js_context;
		js_context.reload();
		get_millennium_info();
	}
	else {
		MessageBoxA(
			GetForegroundWindow(),
			std::format("file: {}\nin the selected skin was not found, therefor can't be loaded.", disk_path).c_str(), "Millennium",
			MB_ICONINFORMATION
		);
	}

	get_millennium_info();
}

class renderer
{
public:

	static void create_card(const char* name, const char* description, int index)
	{
		// Calculate the grid layout
		ImVec2 availableSpace = ImGui::GetContentRegionAvail();
		float desiredItemSize = 150.0f; // Desired size for each child box
		int columns = static_cast<int>(availableSpace.x / desiredItemSize);
		if (columns < 1)
			columns = 1; // Ensure there's at least one column

		float actualItemSize = (availableSpace.x - (columns - 1) * ImGui::GetStyle().ItemSpacing.x) / columns;

		if (index > 0 && index % columns != 0)
			ImGui::SameLine();

		ImGui::BeginChild(("Child " + std::to_string(index)).c_str(), ImVec2(actualItemSize, desiredItemSize), true, ImGuiWindowFlags_None);
		{
			ImGui::BeginChild(("child_image " + std::to_string(index)).c_str(), ImVec2(rx, ry - 25), true, ImGuiWindowFlags_None);
			{

			}
			ImGui::EndChild();

			//ImGui::Text(name);
			ImGui::Button("Apply", ImVec2(rx, 23));
		}
		ImGui::EndChild();
	}

	static void create_library_card(nlohmann::basic_json<>& skin, int index)
	{
		bool popped = false;
		//remove window padding to allow the title bar child to hug the inner sides
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		//get heigth of description text so we can base the height of the modal relative to that
		float desc_height = ImGui::CalcTextSize(skin["description"].get<std::string>().c_str(), 0, false, ImGui::GetContentRegionAvail().x - 50).y;

		if (ImGui::BeginChild(std::format("card_child_container_{}", index).c_str(), ImVec2(rx, 50 + desc_height), true, ImGuiWindowFlags_ChildWindow))
		{		
			//reset the window padding rules
			ImGui::PopStyleVar();
			popped = true;

			//change the default child window color with RGB[0,1]
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.14f, 1.00f));

			//display the title bar of the current skin 
			ImGui::BeginChild(std::format("skin_header{}", index).c_str(), ImVec2(rx, 33), true);
			{
				//display the theme icon
				ImGui::Image((void*)WindowClass::GetIcon().skin_icon, ImVec2(ry - 1, ry - 1));
				ImGui::SameLine();

				ui::shift::x(-4);

				ImGui::Text(skin.value("name", "null").c_str());

				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - 5, ImGui::GetCursorPosY() + 1));

				//push smaller font interface that we loaded earlier
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), std::format("{} by {}", skin.value("version", "1.0.0"), skin.value("author", "unknown")).c_str());
				//reset the font interface to default
				ImGui::PopFont();

				ImGui::SameLine();

				ui::shift::y(-4);
				ui::shift::x(-4);
				ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 15);
				ImGui::PushItemWidth(10);
				ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.18f, 0.64f, 0.29f, 1));

				if (millennium::current_skin == skin.value("native-name", "null"))
				{
					ImGui::Image((void*)WindowClass::GetIcon().check_mark_checked, ImVec2(20, 20));

					if (ImGui::IsItemHovered())
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

						ImGui::BeginTooltip();
						ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disable this skin for steam.");
						ImGui::EndTooltip();
					}
				}
				else
				{
					ImGui::Image((void*)WindowClass::GetIcon().check_mark_unchecked, ImVec2(20, 20));

					if (ImGui::IsItemHovered())
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						ImGui::SetTooltip("Enable this skin for steam.");
					}
				}

				if (ImGui::IsItemClicked())
				{
					change_steam_skin((nlohmann::basic_json<>&)skin);
				}

				ImGui::PopStyleColor();
			}
			ImGui::EndChild();

			ImGui::PopStyleColor();

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);

			ImGui::BeginChild("###description_child", ImVec2(rx - 45, ry), false);
			{
				ImGui::TextWrapped(skin.value("description", "null").c_str());
			}
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

			if (ImGui::ImageButton(WindowClass::GetIcon().trash_icon, ImVec2(15, 15)))
			{
				MessageBoxA(0, 0, 0, 0);
			}

			ImGui::PopStyleColor(2);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				ImGui::SetTooltip(std::format("Permanently delete '{}' from skins folder", skin.value("name", "null")).c_str());
			}
		}

		if (!popped)
			ImGui::PopStyleVar();

		//if (ImGui::IsWindowHovered(ImGui::GetID(std::format("card_child_container_{}", index).c_str())))
		//{
		//	//ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		//}

		//if (current_skin == native_name)
		//{
		//	ImGui::PopStyleColor();
		//}

		ImGui::EndChild();
	}

	static void library()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("My Library");
		ImGui::PopFont();

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 150);
		ImGui::PushItemWidth(150);

		static char text_buffer[150];
		static bool is_focused = false;
		if (ImGui::IsItemActive() || ImGui::IsItemFocused())
			is_focused = true;
		else
			is_focused = false;

		auto position = ImGui::GetCursorPos();
		ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer));
		static const auto after = ImGui::GetCursorPos();

		if (!is_focused && text_buffer[0] == '\0')
		{
			ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::TextUnformatted("Search Library...");
			ImGui::PopStyleColor();

			ImGui::SetCursorPos(after);
		}

		ImGui::PopItemWidth();

		ImGui::Separator();
		// Get the user skins using the function

		//if (conflicting_skin)
		//{
		//	ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), conflict_message.c_str());
		//}

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, .5f), std::format("({} skins, last from: {} at {})", millennium::skin_data.size(), config.get_steam_skin_path(), millennium::last_synced).c_str());
		ImGui::PopFont();

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 95);
		if (ImGui::ImageButton((void*)WindowClass::GetIcon().reload_icon, ImVec2(18, 18)))
		{
			get_millennium_info();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			ImGui::SetTooltip("refresh skins list.");
		}

		ImGui::SameLine();

		ui::shift::x(-6);

		ImGui::PushItemWidth(60);
		if (ImGui::Button("Add Skin", ImVec2(rx, 25)))
		{
			ShellExecute(NULL, "open", std::string(config.get_steam_skin_path()).c_str(), NULL, NULL, SW_SHOWNORMAL);
		}
		ImGui::PopItemWidth();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			ImGui::SetTooltip("Open Millennium skins folder.");
		}

		ImGui::BeginChild("###library_container", ImVec2(rx, ry), false);
		{
			static const auto to_lwr = [](std::string str) {
				std::string result;
				for (char c : str) {
					result += std::tolower(c);
				}
				return result;
			};

			for (size_t i = 0; i < millennium::skin_data.size(); ++i)
			{
				nlohmann::basic_json<>& skin = millennium::skin_data[i];

				if (to_lwr(skin["name"].get<std::string>()).find(to_lwr(text_buffer)) != std::string::npos)
				{
					create_library_card(skin, i);
				}
			}
		}
		ImGui::EndChild();
	}

	static void store()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("Theme Store | WIP");
		ImGui::PopFont();

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 150);
		ImGui::PushItemWidth(150);

		static char text_buffer[150];
		static bool is_focused = false;
		if (ImGui::IsItemActive() || ImGui::IsItemFocused())
			is_focused = true;
		else
			is_focused = false;

		auto position = ImGui::GetCursorPos();
		ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer));
		static const auto after = ImGui::GetCursorPos();

		if (!is_focused && text_buffer[0] == '\0')
		{
			ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::TextUnformatted("Search for themes...");
			ImGui::PopStyleColor();

			ImGui::SetCursorPos(after);
		}

		ImGui::PopItemWidth();

		ImGui::Separator();

		static const int width = 240;
		static const int height = 135;

		ImGui::SetCursorPos(ImVec2((rx / 2) - (width / 2), (ry / 2) - (height / 2)));

		if (ImGui::BeginChild("###store_page", ImVec2(width, height), false))
		{
			ImGui::Image((void*)WindowClass::GetIcon().icon_no_results, ImVec2(240, 110));	
			ImGui::Text("Still in development. Check back later.");
		}
		ImGui::EndChild();
	}

	static void settings()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("Millennium Settings");
		ImGui::PopFont();

		ImGui::Separator();

		static bool enable_js = registry::get_registry("allow-javascript") == "true";
		static bool force_args = false;

		ui::render_setting(
			"Javascript Execution", 
			"Allow Javascript executions in the skins you use.\nJS can help with plethora of things inside Steam especially when customizing Steams look.\n\nJavascript can be malicious in many ways so\nonly allow JS execution on skins of authors you trust, or have manually reviewed the code",
			enable_js,
			true,
			[=]() {
				registry::set_registry("allow-javascript", enable_js ? "true" : "false");

				steam_js_context js_context;
				js_context.reload();
			}
		);

		ui::shift::y(30);
		ImGui::Text("The following settings are disabled until implemented fully");

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

		ui::render_setting(
			"Force CommandLine Arguments",
			"Allow Javascript executions in the skins you use.\nJS can help with plethora of things inside Steam especially when customizing Steams look.\n\nJavascript can be malicious in many ways so\nonly allow JS execution on skins of authors you trust, or have manually reviewed the code",
			force_args,
			false
		);

		ImGui::Text("> Command Line:");
		ImGui::SameLine();

		static char text_buffer[50] = "-dev ...";
		float rightAlignedPosX = ImGui::GetContentRegionMax().x - 155;
		ImGui::SetCursorPosX(rightAlignedPosX);

		ImGui::PushItemWidth(150);
		ImGui::InputText("", text_buffer, sizeof(text_buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsNoBlank);
		ImGui::PopItemWidth();


		ImGui::Spacing();

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("Steam Settings");
		ImGui::PopFont();
		ImGui::Separator();

		static bool enable_url_bar = false;

		ui::render_setting(
			"Show Store Browser URL",
			"Steam removed the ability to show/hide this feature. all it does is hide the URL bar.",
			enable_url_bar,
			true
		);

		ImGui::Spacing();

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("3rd party extensions");
		ImGui::PopFont();

		ImGui::Separator();

		static bool augmented_steam = false;

		ui::render_setting(
			"Enable Augmented Steam",
			"Injects the browser extension Augmented Steam into the Steam client.\n\nMore information available\nhttps://github.com/IsThereAnyDeal/AugmentedSteam",
			augmented_steam,
			true
		);

		ImGui::PopItemFlag();

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

		ImGui::Text("Made with love by ShadowMonster");

		ImGui::PopFont();
	}

	static void content_panel()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10, 0.10, 0.10, 1.00f));

		if (ImGui::BeginChild("###content_panel", ImVec2(rx, ry), false))
		{
			switch (ui::current_tab_page)
			{
				case 1: store(); break;	
				case 2: library(); break;
				case 3: settings(); break;
			}
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
	}
	static void side_bar_renderer()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10, 0.10, 0.10, 1.00f));

		if (ImGui::BeginChild("###sidebar", ImVec2(150, ry), false))
		{
			const auto check_button = [](const char* name, int index) {
				static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

				if (ui::current_tab_page == index) {
					ImGui::PushStyleColor(ImGuiCol_Button, col);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
					if (ImGui::Button(name, ImVec2(rx, 23))) ui::current_tab_page = index;
					if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					ImGui::PopStyleColor(2);
				}
				else {
					if (ImGui::Button(name, ImVec2(rx, 23))) ui::current_tab_page = index;
					if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				}
			};

			check_button("Store", 1);
			check_button("Library", 2);
			check_button("Settings", 3);

			ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - 25);
			check_button("About", 4);
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
	}

	static void main_callback()
	{
		//render the sidebar
		side_bar_renderer();

		ImGui::SameLine();

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

		ImGui::SameLine();

		//render the content panel
		content_panel();

		//if (conflicting_skin)
		//{
		//	ImGui::OpenPopup(" Millennium - Error");
		//	conflicting_skin = false;
		//}

		CenterModal(ImVec2(250, 125));
		if (ImGui::BeginPopupModal(" Millennium - Error", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			ImGui::TextWrapped(millennium::conflict_message.c_str());

			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize("Close").x - 5);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetStyle().ItemSpacing.y - 65);

			if (ImGui::Button("Close")) {
				ImGui::CloseCurrentPopup();
				millennium::conflicting_skin = false;
			}
			ImGui::EndPopup();
		}
	}
};

void init_main_window()
{
	get_millennium_info();

	WindowClass::WindowTitle({ (char*)"Millennium Steam | by ShadowMonster [sm74]" });
	WindowClass::WindowDimensions(ImVec2({ 850, 600 }));

	std::string title = "###";

	Application::Create(title, &renderer::main_callback);
}
