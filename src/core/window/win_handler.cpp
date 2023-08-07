#define _WINSOCKAPI_   
#include <stdafx.h>
#include <random>

#include <core/window/src/window.hpp>
#include <core/window/win_handler.hpp>
#include <utils/config/config.hpp>

#include <utils/http/http_client.hpp>
#include <core/steam/cef_manager.hpp>
#include <core/injector/event_handler.hpp>

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
	bool showing_patch_log = false;

	nlohmann::json patch_log_data;
	int missing_skins = 0;
}

namespace ui
{
	static int current_tab_page = 2; //library

	namespace region
	{
		inline int x() {
			return ImGui::GetContentRegionAvail().x;
		}
		inline int y() {
			return ImGui::GetContentRegionAvail().y;
		}
	}

	namespace shift
	{
		inline void x(int value) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + value);
		}
		inline void y(int value) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + value);
		}
		inline void right(int item_width) {
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

	void center_modal(ImVec2 size)
	{
		RECT rect;
		GetWindowRect(window_class::get_hwnd(), &rect);

		ImGui::SetNextWindowPos
		(ImVec2((rect.left + (((rect.right - rect.left) / 2) - (size.x / 2))), (rect.top + (((rect.bottom - rect.top) / 2) - (size.y / 2)))));
		ImGui::SetNextWindowSize(ImVec2(size.x, size.y));
	}

	void center(float avail_width, float element_width, float padding = 15.0f)
	{
		ImGui::SameLine((avail_width / 2) - (element_width / 2) + padding);
	}

	void center_text(const char* format, float spacing = 15.0f, ImColor color = 0) {
		center(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(format).x);
		ui::shift::y(spacing);
		ImGui::TextColored(color.Value, format);
	}
}

std::string GetRelativeTime(const std::string& timeStr) {
	// Extract date and time components from the time string
	int year, month, day, hour, minute, second;
	if (sscanf(timeStr.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) != 6) {
		return "Invalid time format";
	}

	// Convert the components into a std::tm structure
	std::tm timeStruct = { 0 };
	timeStruct.tm_year = year - 1900;
	timeStruct.tm_mon = month - 1;
	timeStruct.tm_mday = day;
	timeStruct.tm_hour = hour;
	timeStruct.tm_min = minute;
	timeStruct.tm_sec = second;

	// Convert to time_t and calculate the time difference
	time_t eventTime = mktime(&timeStruct);
	time_t currentTime = time(nullptr);
	int timeDiffInSeconds = static_cast<int>(currentTime - eventTime);

	if (timeDiffInSeconds < 60) {
		return "Just now";
	}
	else if (timeDiffInSeconds < 3600) {
		int minutesAgo = static_cast<int>(timeDiffInSeconds / 60);
		return std::to_string(minutesAgo) + " minutes ago";
	}
	else if (timeDiffInSeconds < 86400) {
		int hoursAgo = static_cast<int>(timeDiffInSeconds / 3600);
		return std::to_string(hoursAgo) + " hours ago";
	}
	else if (timeDiffInSeconds < 604800) {
		int daysAgo = static_cast<int>(timeDiffInSeconds / 86400);
		return std::to_string(daysAgo) + " days ago";
	}
	else if (timeDiffInSeconds < 2592000) {
		int weeksAgo = static_cast<int>(timeDiffInSeconds / 604800);
		return std::to_string(weeksAgo) + " weeks ago";
	}
	else if (timeDiffInSeconds < 31536000) {
		int monthsAgo = static_cast<int>(timeDiffInSeconds / 2592000);
		return std::to_string(monthsAgo) + " months ago";
	}
	else {
		int yearsAgo = static_cast<int>(timeDiffInSeconds / 31536000);
		return std::to_string(yearsAgo) + " years ago";
	}
}


static void display_event(const nlohmann::json& event) {
	// Display the event details here
	//ImGui::Text("Event ID: %s", event["id"].get<std::string>().c_str());
	//ImGui::Text("Type: %s", event["type"].get<std::string>().c_str());
	//ImGui::Text("Actor: %s", event["actor"]["login"].get<std::string>().c_str());
	if (event["payload"].contains("commits"))
	{
		ImGui::Text("Patch Notes from release");

		for (const auto& message : event["payload"]["commits"])
		{
			ImGui::Text(message["message"].get<std::string>().c_str());
		}
	}
	ImGui::Text("Updated: %s", GetRelativeTime(event["created_at"].get<std::string>()).c_str());
	// Display other event details as needed
}

static void display_children(nlohmann::basic_json<>& events) {
	for (const auto& event : events) {

		int height = 35;

		if (event["payload"].contains("commits"))
			height += event["payload"]["commits"].size() * 45;

		if (ImGui::BeginChild(event["id"].get<std::string>().c_str(), ImVec2(0, height), true)) {
			display_event(event);
		}
		ImGui::EndChild();
	}
}

static void get_millennium_info(void)
{
	millennium::missing_skins = 0;

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
		try
		{
			if (entry.path().filename().string().find(".millennium.json") == std::string::npos)
				continue;

			nlohmann::json data; {
				std::ifstream skin_json_file(entry.path().string()); skin_json_file >> data;
			}

			nlohmann::basic_json<> result = nlohmann::json::parse(http::get(data["skin-json"]));

			result["github"]["username"] = data["gh_username"];
			result["github"]["repo"] = data["gh_repo"];

			result["name"] = result.value("name", entry.path().filename().string()).c_str();
			result["native-name"] = entry.path().filename().string();
			result["description"] = result.value("description", "no description yet.").c_str();
			result["remote"] = true;

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
		catch (std::exception& exception)
		{
			std::cout << "no internet connection?" << std::endl;
			millennium::missing_skins++;
		}
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
		data["remote"] = false;

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

	std::string remote_skin = std::format("{}/{}", config.get_steam_skin_path(), queried_skin);
	std::string disk_path = std::format("{}/{}/skin.json", config.get_steam_skin_path(), queried_skin);

	if (millennium::current_skin == skin.value("native-name", "null")) {
		queried_skin = "default";
	}

	//whitelist the default skin
	if (queried_skin == "default" || (std::filesystem::exists(disk_path) || std::filesystem::exists(remote_skin)))
	{	
		std::cout << "updating selected skin." << std::endl;

		//update registry key holding the active-skin selection
		registry::set_registry("active-skin", queried_skin);
		skin_config::skin_change_events::get_instance().trigger_change();

		steam_js_context js_context;
		js_context.reload();

		if (steam::get_instance().params.has("-silent"))
		{
			MessageBoxA(
				GetForegroundWindow(),
				"Steam is launched in -silent mode so you need to open steam again from the task tray for it to re-open", "Millennium",
				MB_ICONINFORMATION
			);
		}
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
				if (skin["remote"])
				{
					ImGui::Image((void*)window_class::get_ctx().icon_remote_skin, ImVec2(ry - 1, ry - 1));
					if (ImGui::IsItemHovered())
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

						ImGui::BeginTooltip();

						ImGui::Text("This is a remotely hosted Steam Skin, meaning it is not stored on your PC");
						ImGui::Text("What does this mean?");

						ImGui::Separator();

						ImGui::Text("Skins can be updated and the code can be changed by the developer AT ANY TIME");

						ImGui::TextColored(ImVec4(1, 0, 0, 1), "This means there COULD be potential MALICIOUS updates without you knowing");
						ImGui::Text("Make sure you trust the skin developer before continuing.");

						ImGui::Separator();

						ImGui::Text("NOTE: remote skins wont be able to load properly offline unless you download them.");
						ImGui::EndTooltip();
					}
				}
				else
					ImGui::Image((void*)window_class::get_ctx().skin_icon, ImVec2(ry - 1, ry - 1));

				ImGui::SameLine();

				ui::shift::x(-4);

				ImGui::Text(skin.value("name", "null").c_str());

				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - 5, ImGui::GetCursorPosY() + 1));

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), std::format("{} by {}", skin.value("version", "1.0.0"), skin.value("author", "unknown")).c_str());

				ImGui::PopFont();

				ImGui::SameLine();

				ui::shift::y(-4);
				ui::shift::x(-4);
				if (skin["remote"])
					ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 45);
				else
					ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 17);

				ImGui::PushItemWidth(10);
				ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.18f, 0.64f, 0.29f, 1));

				if (skin["remote"])
				{
					ImGui::Image((void*)window_class::get_ctx().more_icon, ImVec2(20, 20));
					if (ImGui::IsItemHovered())
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					}
					if (ImGui::IsItemClicked())
					{
						ImGui::OpenPopup("MorePopup");
					}

					ImGui::SameLine();
					ui::shift::x(-5);
				}

				if (ImGui::BeginPopup("MorePopup")) {
					// The content inside the drop-down menu goes here
					if (ImGui::Selectable("View Source")) {
						ShellExecute(NULL, "open", std::format("https://github.com/{}/{}/", skin["github"]["username"].get<std::string>(), skin["github"]["repo"].get<std::string>()).c_str(), NULL, NULL, SW_SHOWNORMAL);
					}
					if (ImGui::Selectable("View Update History")) {
						ImGui::OpenPopup(" Millennium - Update History");
						std::make_shared<std::thread>([&]() {			
							millennium::showing_patch_log = true;
							std::string url = std::format("https://api.github.com/repos/{}/{}/events", skin["github"]["username"].get<std::string>(), skin["github"]["repo"].get<std::string>());
							millennium::patch_log_data = nlohmann::json::parse(http::get(url));
						})->detach();
					}

					ImGui::EndPopup();
				}

				if (millennium::current_skin == skin.value("native-name", "null"))
				{
					ImGui::Image((void*)window_class::get_ctx().check_mark_checked, ImVec2(20, 20));

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
					ImGui::Image((void*)window_class::get_ctx().check_mark_unchecked, ImVec2(20, 20));

					if (ImGui::IsItemHovered())
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						ImGui::SetTooltip("Enable this skin for steam.");
					}
				}

				if (ImGui::IsItemClicked())
				{
					std::cout << ("WAdawdawdawdawdawdawd") << std::endl;

					std::make_shared<std::thread>([=]() {
						change_steam_skin((nlohmann::basic_json<>&)skin);
					})->detach();
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

			if (ImGui::ImageButton(window_class::get_ctx().trash_icon, ImVec2(15, 15)))
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

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, .5f), std::format("({} skins, last from: {} at {})", millennium::skin_data.size(), config.get_steam_skin_path(), millennium::last_synced).c_str());
		ImGui::PopFont();

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 95);
		if (ImGui::ImageButton((void*)window_class::get_ctx().reload_icon, ImVec2(18, 18)))
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
			if (millennium::missing_skins > 0)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), std::format("{} missing skin(s) from listings (no connection)", millennium::missing_skins).c_str());
			}

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

		if (ImGui::IsItemActive() || ImGui::IsItemFocused()) {
			is_focused = true;
		}
		else {
			is_focused = false;
		}

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
			ImGui::Image((void*)window_class::get_ctx().icon_no_results, ImVec2(240, 110));	
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

	static void console_tab()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("Output Logs");
		ImGui::PopFont();

		ImGui::Separator();

		if (ImGui::BeginChild("###console_logs", ImVec2(rx, ry), false))
		{
			for (const auto& message : output_log)
			{
				//bool is_patch_message = message.find("match") != std::string::npos;
				bool is_error = message.find("fail") != std::string::npos;
				//bool is_info = message.find("info") != std::string::npos;

				//if (is_patch_message) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.0f, 0.0f, 0.5f));
				if (is_error) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				//if (is_info) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.424f, 0.0f, 1.0f, 0.5f));

				ImGui::TextWrapped(message.c_str());
				if (is_error) ImGui::PopStyleColor();
			}
		}
		ImGui::EndChild();
	}

	static void steam_websocket()
	{
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
		ImGui::Text("Steam Client WebSocket Messages");
		ImGui::PopFont();

		ImGui::Separator();

		if (ImGui::BeginChild("###websocket_msgs", ImVec2(rx, ry), false))
		{
			for (const auto& message : socket_log)
			{
				ImGui::TextWrapped(message.c_str());
			}
		}
		ImGui::EndChild();
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
				case 4: console_tab(); break;
				case 5: steam_websocket(); break;
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
			ui::center(rx, 50, 0);
			ui::shift::y(15);
			ImGui::Image(window_class::get_ctx().icon, ImVec2(50, 50));
			ui::shift::y(15);

			//ImGui::Image((void*)WindowClass::GetIcon().close, ImVec2(10, 10));
			//ImGui::SameLine();
			//ui::shift::x(-6);
			//ImGui::Image((void*)WindowClass::GetIcon().max, ImVec2(10, 10));

			//if (ImGui::IsItemClicked())
			//{
			//	ShowWindow(WindowClass::GetHWND(), SW_SHOWMAXIMIZED);
			//	set_mouse_down();
			//}

			//ImGui::SameLine();
			//ui::shift::x(-6);
			//ImGui::Image((void*)WindowClass::GetIcon().min, ImVec2(10, 10));

			//if (ImGui::IsItemClicked())
			//{
			//	ShowWindow(WindowClass::GetHWND(), SW_SHOWMINIMIZED);
			//	set_mouse_down();
			//}

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

			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "MILLENNIUM");
			ImGui::PopFont();
			check_button("Store", 1);
			check_button("Library", 2);
			check_button("Settings", 3);

			ImGui::Spacing();
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "DEVELOPER");
			ImGui::PopFont();

			check_button("Console", 4);
			check_button("Steam:WS", 5);

			ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - 21);

			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

			if (ImGui::ImageButton(window_class::get_ctx().exit, ImVec2(15, 15)))
			{
				PostQuitMessage(0);
			}

			ImGui::PopStyleColor();

			if (ImGui::IsItemHovered())
			{
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				ImGui::SetTooltip("Close Millennium");
			}
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

		if (millennium::showing_patch_log)
		{
			ImGui::OpenPopup(" Millennium - Update History");
		}

		ui::center_modal(ImVec2(ImGui::GetMainViewport()->WorkSize.x / 1.5, ImGui::GetMainViewport()->WorkSize.y / 1.5));
		if (ImGui::BeginPopupModal(" Millennium - Update History", &millennium::showing_patch_log, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			if (millennium::patch_log_data == 0)
			{
				ImGui::Text("Loading details...");
			}
			else
			{		
				try {
					display_children(millennium::patch_log_data);
				}
				catch (nlohmann::json::exception& ex) {
					console.err(std::format("error occured while trying to display patch log data {}", ex.what()));
				}
			}
			ImGui::EndPopup();
		}

		//if (millennium::showing_patch_log)
		//{
		//	ImGui::Begin("This is details window", &millennium::showing_patch_log);

		//	if (millennium::patch_log_data == 0)
		//	{
		//		ImGui::Text("Loading details...");
		//	}
		//	else
		//	{
		//		display_children(millennium::patch_log_data);
		//	}

		//	ImGui::End();
		//}
	}
};

void init_main_window()
{
	get_millennium_info();

	window_class::wnd_title({ (char*)"Valve.Millennium.Client" });
	window_class::wnd_dimension(ImVec2({ 850, 600 }));

	std::string title = "###";

	Application::Create(title, &renderer::main_callback);
}
