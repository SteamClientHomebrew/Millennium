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

#include <window/interface/notify/imnotify.h>

ImVec4 HexToImVec4(const char* hexColor) {
	ImVec4 result(0.0f, 0.0f, 0.0f, 1.0f); // Default to black with full opacity

	if (hexColor[0] == '#' && (strlen(hexColor) == 7 || strlen(hexColor) == 9)) {
		unsigned int hexValue = 0;
		int numParsed = sscanf(hexColor + 1, "%x", &hexValue);

		if (numParsed == 1) {
			if (strlen(hexColor) == 7) {
				result.x = static_cast<float>((hexValue >> 16) & 0xFF) / 255.0f;
				result.y = static_cast<float>((hexValue >> 8) & 0xFF) / 255.0f;
				result.z = static_cast<float>(hexValue & 0xFF) / 255.0f;
			}
			else if (strlen(hexColor) == 9) {
				result.x = static_cast<float>((hexValue >> 24) & 0xFF) / 255.0f;
				result.y = static_cast<float>((hexValue >> 16) & 0xFF) / 255.0f;
				result.z = static_cast<float>((hexValue >> 8) & 0xFF) / 255.0f;
				result.w = static_cast<float>(hexValue & 0xFF) / 255.0f;
			}
		}
	}

	return result;
}

std::string ImVec4ToHex(const ImVec4& color) {
	// Clamp the color values to the valid range [0, 1]
	ImVec4 clampedColor;
	clampedColor.x = ImClamp(color.x, 0.0f, 1.0f);
	clampedColor.y = ImClamp(color.y, 0.0f, 1.0f);
	clampedColor.z = ImClamp(color.z, 0.0f, 1.0f);
	clampedColor.w = ImClamp(color.w, 0.0f, 1.0f);

	// Convert the clamped color values to hexadecimal
	unsigned char r = static_cast<unsigned char>(clampedColor.x * 255.0f);
	unsigned char g = static_cast<unsigned char>(clampedColor.y * 255.0f);
	unsigned char b = static_cast<unsigned char>(clampedColor.z * 255.0f);
	unsigned char a = static_cast<unsigned char>(clampedColor.w * 255.0f);

	std::stringstream ss;
	ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(r);
	ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(g);
	ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);

	// Optionally, include the alpha channel if it's not fully opaque
	if (a < 255) {
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(a);
	}

	return ss.str();
}

struct render
{
private:
	bool m_downloadInProgess = false;
	std::string m_downloadStatus;
	
	nlohmann::basic_json<> m_itemSelectedSource;

	const void listButton(const char* name, int index) {
		static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

		if (ui::current_tab_page == index) {
			if (ImGui::Selectable(std::format(" {}", name).c_str())) ui::current_tab_page = index;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			if (ImGui::Selectable(name)) ui::current_tab_page = index;
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (ImGui::IsItemClicked(1)) ImGui::OpenPopup(std::format("selectable_{}", index).c_str());
			ImGui::PopStyleColor(1);
		}
	};

	std::string sanitizeDirectoryName(const std::string& input) {
		return std::regex_replace(input, std::regex("[^a-zA-Z0-9-_.~]"), "");
	}
	
	void downloadFolder(const nlohmann::json& folderData, const std::filesystem::path& currentDir) 
	{
		std::filesystem::create_directories(currentDir);

		for (const auto& item : folderData) 
		{		
			try {
				if (item["type"] == "dir")
				{
					std::filesystem::path dirPath = currentDir / item["name"];
					std::filesystem::create_directories(dirPath);

					downloadFolder(nlohmann::json::parse(http::get(item["url"])), dirPath);
				}
				else if (item["type"] == "file" && !item["download_url"].is_null())
				{
					const std::string fileName = item["name"].get<std::string>();
					const std::filesystem::path filePath = currentDir / fileName;

					m_downloadStatus = std::format("downloading {}", fileName);
					this->writeFileBytesSync(filePath, http::get_bytes(item["download_url"].get<std::string>().c_str()));
				}
			}
			catch (const nlohmann::detail::exception& excep) {
				if (item.contains("message")) {
					const auto message = item["message"].get<std::string>();
					MsgBox(std::format("Message:\n", message).c_str(), "Error contacting GitHub API", MB_ICONERROR);
				}

				console.err(std::format("Error downloading/updating a skin. message: {}", excep.what()));
			}
		}
	}

	void downloadTheme(const nlohmann::json& skinData) {
		std::filesystem::path skinDir = std::filesystem::path(config.getSkinDir()) / sanitizeDirectoryName(skinData["name"].get<std::string>());

		try {
			nlohmann::json folderData = nlohmann::json::parse(http::get(skinData["source"]));
			downloadFolder(folderData, skinDir);
		}
		catch (const http_error& exception) {
			console.log(std::format("download theme exception: {}", exception.what()));
		}
	}

public:

	void writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
	{
		console.log(std::format("writing file to: {}", filePath.string()));

		std::ofstream fileStream(filePath, std::ios::binary);
		if (!fileStream)
		{
			console.log(std::format("Failed to open file for writing: {}", filePath.string()));
			return;
		}

		fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

		if (!fileStream)
		{
			console.log(std::format("Error writing to file: {}", filePath.string()));
		}

		fileStream.close();
	}

	struct updateItem
	{
		std::string status;
		int id = -1;
	};

	updateItem updatingItem;
	nlohmann::json m_editObj;
	bool m_editMenuOpen = false;

	struct ColorInfo {
		std::string name;
		ImVec4 color;
		std::string comment;
	};

	std::vector<ColorInfo> colorList;
	std::string colorFileBuffer;
	std::string colorFilePathBuffer;

	std::vector<ColorInfo> parseColors(const std::string& input) {
		std::vector<ColorInfo> colors;

		// Define a regular expression pattern to match color definitions
		std::regex pattern("--([a-zA-Z-]+):\\s*#([0-9a-fA-F]+);\\s*/\\*([^*]*)\\*/");

		// Create an iterator to search for matches in the input string
		std::sregex_iterator iter(input.begin(), input.end(), pattern);
		std::sregex_iterator end;

		while (iter != end) {
			ColorInfo color;
			color.name = (*iter)[1].str();
			color.color = ImVec4(HexToImVec4(std::format("#{}", (*iter)[2].str()).c_str()));
			color.comment = (*iter)[3].str();
			colors.push_back(color);
			++iter;
		}

		return colors;
	}

	void updateColor(std::string& input, const std::string& colorName, const std::string& newColorCode) {
		std::regex pattern("--" + colorName + ":\\s*#([0-9a-fA-F]+);");
		std::smatch match;

		if (std::regex_search(input, match, pattern)) {
			// Replace the existing color code with the new color code
			input.replace(match.position(1), match.length(1), newColorCode);
		}
	}

	const std::string readFileSync(std::string path)
	{
		std::basic_ifstream<char, std::char_traits<char>> filePath(path);

		std::string file_content((std::istreambuf_iterator<char>(filePath)), std::istreambuf_iterator<char>());

		return file_content;
	}

	void openPopupMenu(nlohmann::basic_json<>& skin)
	{
		//try to parse colors from the skin object
		if (skin.contains("GlobalsColors"))
		{
			const std::string m_activeSkin = Settings::Get<std::string>("active-skin");

			colorFilePathBuffer = std::format("{}/{}/{}", config.getSkinDir(), m_activeSkin, skin["GlobalsColors"].get<std::string>());

			if (std::filesystem::exists(colorFilePathBuffer))
			{
				console.succ("Color file was found. Trying to parse.");

				colorFileBuffer = this->readFileSync(colorFilePathBuffer);
				colorList = parseColors(colorFileBuffer);

				for (const ColorInfo& color : colorList) {
					std::cout << "Name: " << color.name << std::endl;
					std::cout << "Code: " << color.color.x << std::endl;


					std::cout << "Comment: " << color.comment << std::endl;
				}
			}
			else {
				console.err(std::format("Color file '{}' couldn't be located. Tried path: '{}'",
					skin["GlobalsColors"].get<std::string>(), colorFilePathBuffer));
			}
		}
		else {
			console.log("Theme doesn't have a GlobalColors file");
		}

		m_editObj = skin;
		m_editMenuOpen = true;
	}

	void createLibraryListing(nlohmann::basic_json<>& skin, int index)
	{
		const std::string m_skinName = skin["native-name"];

		static bool push_popped = false;
		static int hovering;
		//manages if the edit skit popupmodal is open
		static bool editSkinIndex = -1;

		bool popped = false;
		//remove window padding to allow the title bar child to hug the inner sides
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		//get heigth of description text so we can base the height of the modal relative to that
		float desc_height = ImGui::CalcTextSize(skin["description"].get<std::string>().c_str(), 0, false, ImGui::GetContentRegionAvail().x - 50).y;

		if (hovering == index) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
			push_popped = true;
		}
		else ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		static bool updateButtonHovered = false;

		if (m_Client->m_currentSkin == m_skinName)
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		else
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 0.0f));

		bool requiresUpdate = skin["update_required"].get<bool>();

		ImGui::BeginChild(std::format("card_child_container_{}", index).c_str(), ImVec2(rx, 85 + desc_height), true, ImGuiWindowFlags_ChildWindow);
		{
			//reset the window padding rules
			ImGui::PopStyleVar();
			popped = true;
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6);
			ImGui::PopStyleColor();

			//display the title bar of the current skin 
			ImGui::BeginChild(std::format("skin_header{}", index).c_str(), ImVec2(rx, 33), true);
			{
				ImGui::Image((void*)Window::iconsObj().skin_icon, ImVec2(ry - 1, ry - 1));

				ImGui::SameLine();

				ui::shift::x(-4);

				ImGui::Text(skin.value("name", "null").c_str());

				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - 5, ImGui::GetCursorPosY() + 1));

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), std::format("{} by {}", skin.value("version", "1.0.0"), skin.value("author", "unknown")).c_str());

				ImGui::SameLine();
				ui::shift::right(35);
				ui::shift::y(-3);

				ImGui::PopFont();
			}
			ImGui::EndChild();
			ImGui::PopStyleVar();

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);

			ImGui::BeginChild("###description_child", ImVec2(rx - 10, ry - 33), false);
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.0));
				ImGui::TextWrapped(skin.value("description", "null").c_str());
				ImGui::PopStyleColor();
			}
			ImGui::EndChild();

			if (requiresUpdate) 
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

				ui::shift::y(-5);
				ui::shift::right(190);

				if (ImGui::Button("Edit", ImVec2(50, 25)))
				{
					this->openPopupMenu(skin);
				}
				if (ImGui::IsItemHovered()) {
					updateButtonHovered = true;
				}
				else {
					updateButtonHovered = false;
				}
				ImGui::SameLine();
				ui::shift::x(-4);

				if (ImGui::Button(updatingItem.id == index ? updatingItem.status.c_str() : "Update Now!", ImVec2(125, 25))) {
					updatingItem.id = index;
					updatingItem.status = "Updating...";

					std::thread([&]() { 
						this->downloadTheme(skin);
						m_Client->parseSkinData(); 

						updatingItem.id = -1;
					}).detach();
				}
				if (ImGui::IsItemHovered()) {
					updateButtonHovered = true;
				}
				else {
					updateButtonHovered = false;
				}
				ImGui::PopStyleColor(2);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

				ui::shift::y(-5);
				ui::shift::right(55);

				if (ImGui::Button("Edit", ImVec2(50, 25)))
				{
					this->openPopupMenu(skin);
				}	
				if (ImGui::IsItemHovered()) {
					updateButtonHovered = true;
				}
				else {
					updateButtonHovered = false;
				}
				ImGui::PopStyleColor(2);
			}	
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
			ImGui::OpenPopup("MB2 Context Menu");
		}

		bool m_isHovered = false;

		if (ImGui::BeginPopupContextItem("MB2 Context Menu"))
		{
			if (ImGui::IsItemHovered()) m_isHovered = true;
			if (ImGui::MenuItem("Delete"))
			{
				std::string disk_path = std::format("{}/{}", config.getSkinDir(), skin["native-name"].get<std::string>());

				console.log(std::format("deleting skin {}", disk_path));

				if (std::filesystem::exists(disk_path)) {

					try {
						std::filesystem::remove_all(std::filesystem::path(disk_path));
					}
					catch (const std::exception& ex) {
						MsgBox(std::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
					}
				}

				m_Client->parseSkinData();
			}
			if (ImGui::IsItemHovered()) m_isHovered = true;

			ImGui::EndPopup();
		}

		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
			std::thread([=]() {
				m_Client->changeSkin((nlohmann::basic_json<>&)skin);
			}).detach();
		}

		if (!m_isHovered && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if (!updateButtonHovered) {
				hovering = index;
			} else {
				hovering = -1;
			}

			push_popped = false;
		}
		else if (push_popped) hovering = -1;

		ImGui::PopStyleColor();
		ImGui::EndChild();
	}
	void library_panel()
	{
		//if (ImGui::Button("Testing Button"))
		//{
		//	clientMessagePopup("Hello", "Hi!");
		//}

		if (m_Client->m_missingSkins > 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, .5f), std::format("{} missing remote skin(s). couldn't connect to endpoint", m_Client->m_missingSkins).c_str());
			ImGui::SameLine();
		}
		if (m_Client->m_unallowedSkins > 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, .5f), std::format("{} missing remote skin(s). networking not allowed", m_Client->m_unallowedSkins).c_str());
			ImGui::SameLine();
		}

		static char text_buffer[150];

		ImGui::BeginChild("HeaderDrag", ImVec2(rx, 35), false);
		{
			if (ImGui::IsWindowHovered())
			{
				g_headerHovered = true;
			}
			else
			{
				g_headerHovered = false;
			}

			ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 150);
			ImGui::PushItemWidth(150);

			auto position = ImGui::GetCursorPos();
			ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer));
			static const auto after = ImGui::GetCursorPos();

			if (ImGui::IsItemHovered()) {
				g_headerHovered = false;
			}


			static bool is_focused = false;
			if (ImGui::IsItemActive() || ImGui::IsItemFocused())
				is_focused = true;
			else
				is_focused = false;

			if (!is_focused && text_buffer[0] == '\0')
			{
				ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::TextUnformatted("Search Library...");
				ImGui::PopStyleColor();

				ImGui::SetCursorPos(after);
			}

			ImGui::PopItemWidth();
		}
		ImGui::EndChild();

		auto worksize = ImGui::GetMainViewport()->WorkSize;

		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		ui::center(rx, child_width, 8);

		static float contentHeight = 100.0f; // Initialize with a minimum height

		ImGui::BeginChild("###library_container", ImVec2(child_width, contentHeight), true, ImGuiWindowFlags_AlwaysAutoResize);
		{
			static const auto to_lwr = [](std::string str) {
				std::string result;
				for (char c : str) {
					result += std::tolower(c);
				}
				return result;
			};

			bool skinSelected = false;

			for (size_t i = 0; i < m_Client->skinData.size(); ++i)
			{
				nlohmann::basic_json<>& skin = m_Client->skinData[i];

				if (skin.value("native-name", std::string()) == m_Client->m_currentSkin)
				{
					skinSelected = true;

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
					{
						ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SELECTED:");
					}
					ImGui::PopFont();

					createLibraryListing(skin, i);

					ImGui::Spacing();
					ImGui::Spacing();
				}
			}

			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "LIBRARY:");
			}
			ImGui::PopFont();

			ImGui::SameLine();

			ui::shift::right(145);

			if (ImGui::Button(" ... ")) {
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

			static int p_sortMethod = 0;

			static const char* items[] = { "All Items", "Installed", "Cloud" };

			ImGui::PushItemWidth(115);
			if (ImGui::Combo(std::format("Sort").c_str(), &p_sortMethod, items, IM_ARRAYSIZE(items)));
			ImGui::PopItemWidth();

			if (!skinSelected ? m_Client->skinData.empty() : m_Client->skinData.size() - 1 <= 0) {
				ui::shift::y(ry / 2 - 135);

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
				for (size_t i = 0; i < m_Client->skinData.size(); ++i)
				{
					nlohmann::basic_json<>& skin = m_Client->skinData[i];

					if (p_sortMethod == 1 && skin["remote"])
						continue;

					if (p_sortMethod == 2 && !skin["remote"])
						continue;

					if (skin["native-name"].get<std::string>() != m_Client->m_currentSkin &&
						to_lwr(skin["name"].get<std::string>()).find(to_lwr(text_buffer)) != std::string::npos)
					{
						createLibraryListing(skin, i);
					}
				}
			}

			contentHeight = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
		}
		ImGui::EndChild();
	}
	void settings_panel()
	{
		ImGui::BeginChild("HeaderDrag", ImVec2(rx, 35), false);
		{
			if (ImGui::IsWindowHovered())
			{
				g_headerHovered = true;
			}
			else
			{
				g_headerHovered = false;
			}
		}
		ImGui::EndChild();

		static steam_js_context SteamJSContext;

		auto worksize = ImGui::GetMainViewport()->WorkSize;
		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		ui::center(rx, child_width, -1);
		ImGui::BeginChild("settings_panel", ImVec2(child_width, ry), false);
		{
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

			static bool enable_js = Settings::Get<bool>("allow-javascript");

			ui::render_setting(
				"Javascript Execution", "Allow Javascript executions in the skins you use.\nJS can help with plethora of things inside Steam especially when customizing Steams look.\n\nJavascript can be malicious in many ways so\nonly allow JS execution on skins of authors you trust, or have manually reviewed the code",
				enable_js, false,
				[=]() {
					Settings::Set("allow-javascript", enable_js);
					SteamJSContext.reload();
				}
			);

			ImGui::Spacing(); ImGui::Spacing();

			static bool enable_store = Settings::Get<bool>("allow-store-load");

			ui::render_setting(
				"Enable Networking", "Controls whether HTTP requests are allowed which affects the Millennium Store and cloud hosted themes in case you want to opt out.\nThe store/millennium collects no identifiable data on the user, in fact, it ONLY collects download count.\n\nMillennium stays completely offline with this disabled.",
				enable_store, false,
				[=]() { Settings::Set("allow-store-load", enable_store); }
			);


			ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing();

			static const char* items[] = { "Top Left"/*0*/, "Top Right"/*1*/, "Bottom Left"/*2*/, "Bottom Right"/*3*/ };

			static int notificationPos = nlohmann::json::parse(SteamJSContext.exec_command("SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position"))["result"]["value"];

			ui::render_setting_list(
				"Client Notifications Position", "Adjusts the position of the client location instead of using its native coordinates. Displaying in the top corners is slightly broken",
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
		}
		ImGui::EndChild();
	}
	void loadSelection(MillenniumAPI::resultsSchema& item)
	{
		//m_Client->releaseImages();

		m_Client->m_resultsSchema = item;
		m_Client->getRawImages(item.v_images);
		m_Client->b_showingDetails = true;
		m_Client->m_imageIndex = 0;

		console.log(item.skin_json);

		std::thread([=]() {

			try
			{
				std::string skinJsonResponse = http::get(m_Client->m_resultsSchema.skin_json);

				if (!nlohmann::json::accept(skinJsonResponse))
				{
					MsgBox(std::format("Json couldn't be parsed that was received from the remote server\n{}", 
						m_Client->m_resultsSchema.skin_json).c_str(), "Millennium", MB_ICONERROR);
					return;
				}

				this->m_itemSelectedSource = nlohmann::json::parse(skinJsonResponse);
			}
			catch (const http_error&) {
				MsgBox("Couldn't GET the skin data from the remote server", "Millennium", MB_ICONERROR);
				return;
			}
		}).detach();
	}
	void create_card(MillenniumAPI::resultsSchema& item, int index)
	{
		static bool push_popped = false;
		static int hovering;

		try
		{
			ImVec2 availableSpace = ImGui::GetContentRegionAvail();

			float desiredItemSize = (rx / 3) > 400 ? 400 : rx / 3;
			int columns = static_cast<int>(availableSpace.x / desiredItemSize);
			if (columns < 1) {
				columns = 1;
			}

			float actualItemSize = (availableSpace.x - (columns - 1) * ImGui::GetStyle().ItemSpacing.x) / columns;

			if (index > 0 && index % columns != 0) ImGui::SameLine();

			if (hovering == index)
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
				push_popped = true;
			}
			else
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

			int height = item.image_list.height;
			int width = item.image_list.width;

			image::size result = image::maintain_aspect_ratio(width, height, (int)actualItemSize, (int)desiredItemSize);

			ImGui::BeginChild(("Child " + std::to_string(index)).c_str(), ImVec2(actualItemSize, result.height + 80.0f), true, ImGuiWindowFlags_None);
			{
				ImGui::PopStyleVar();

				if (item.image_list.texture != nullptr && item.image_list.texture->AddRef())
				{
					ImGui::Image(item.image_list.texture, ImVec2((float)result.width, (float)result.height));
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					hovering = index;
					push_popped = false;
				}
				else if (push_popped) hovering = -1;

				if (ImGui::IsItemClicked())
				{
					loadSelection(item);
				}

				ui::shift::x(10);

				ImGui::BeginChild("###listing_description", ImVec2(rx, ry), false);
				{
					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
					ImGui::Text(item.name.c_str());
					ImGui::PopFont();
					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

					ImGui::SameLine();

					float width = ImGui::CalcTextSize(std::to_string(item.download_count).c_str()).x;

					ImGui::SameLine();
					ui::shift::right(30 + (int)width);
					//ui::shift::y(4);

					ImGui::Image(Window::iconsObj().download, ImVec2(15, 15));

					ImGui::SameLine();
					ui::shift::x(-6);
					//ui::shift::y(-4);
					ImGui::Text(std::to_string(item.download_count).c_str());

					ImGui::Text("by %s", item.gh_username.c_str());

					ImVec2 textSize = ImGui::CalcTextSize(item.description.c_str());
					ImVec2 availableSpace = ImGui::GetContentRegionAvail();

					if (textSize.x > availableSpace.x) {
						float ellipsisWidth = ImGui::CalcTextSize("...").x;
						float availableWidth = availableSpace.x - ellipsisWidth - 20;

						size_t numChars = 0;
						while (numChars < strlen(item.description.c_str()) && ImGui::CalcTextSize(item.description.c_str(), item.description.c_str() + numChars).x <= availableWidth) {
							numChars++;
						}

						ImGui::TextUnformatted(item.description.c_str(), item.description.c_str() + numChars);
						ImGui::SameLine(0);
						ui::shift::x(-12);
						ImGui::Text("...");
					}
					else {
						ImGui::TextUnformatted(item.description.c_str());
					}

					ImGui::PopStyleColor();
					ImGui::PopFont();
				}
				ImGui::EndChild();

				if (ImGui::IsItemClicked())
				{
					loadSelection(item);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					hovering = index;
					push_popped = false;
				}
				else if (push_popped) hovering = -1;
			}
			ImGui::EndChild();

			if (ImGui::IsItemClicked())
			{
				loadSelection(item);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				hovering = index;
				push_popped = false;
			}
			else if (push_popped) hovering = -1;


			ImGui::PopStyleColor();
		}
		catch (std::exception& ex)
		{
			console.err(ex.what());
		}
	}
	void store_panel()
	{
		ImGui::BeginChild("HeaderDrag", ImVec2(rx, 35), false);
		{
			if (ImGui::IsWindowHovered())
			{
				g_headerHovered = true;
			}
			else
			{
				g_headerHovered = false;
			}

			ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 150);
			ImGui::PushItemWidth(150);

			static char text_buffer[150];

			auto position = ImGui::GetCursorPos();
			if (ImGui::InputText("##myInput", text_buffer, sizeof(text_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (strlen(text_buffer) > 0)
					api->retrieve_search(text_buffer);
			}
			static const auto after = ImGui::GetCursorPos();

			if (ImGui::IsItemHovered()) {
				g_headerHovered = false;
			}

			static bool is_focused = false;

			if (ImGui::IsItemActive() || ImGui::IsItemFocused()) {
				is_focused = true;
			}
			else {
				is_focused = false;
			}

			if (!is_focused && text_buffer[0] == '\0')
			{
				ImGui::SetCursorPos(ImVec2(position.x + 5, position.y + 2));

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				ImGui::TextUnformatted("Search for themes...");
				ImGui::PopStyleColor();

				ImGui::SetCursorPos(after);
			}

			ImGui::PopItemWidth();
		}
		ImGui::EndChild();

		auto worksize = ImGui::GetMainViewport()->WorkSize;

		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		if (api->get_query().size() == 0)
		{
			static const int width = 240;
			static const int height = 80;

			ImGui::SetCursorPos(ImVec2((rx / 2) - (width / 2), (ry / 2) - (height / 2)));

			if (ImGui::BeginChild("###store_page", ImVec2(width, height), false))
			{
				static int r = ImClamp(35, 0, 255);
				static int g = ImClamp(35, 0, 255);
				static int b = ImClamp(35, 0, 255);
				static int a = ImClamp(255, 0, 255);

				static ImU32 packedColor = static_cast<ImU32>((a << 24) | (b << 16) | (g << 8) | r);

				ImGui::Spacing();
				ui::center(rx, 30.0f, -9);
				ImGui::Spinner("###loading_spinner", 25.0f, 7, packedColor);

			}
			ImGui::EndChild();
		}
		else
		{
			if (!m_Client->b_showingDetails)
			{
				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::Spacing();

				ui::center(rx, child_width, 8);

				ImGui::BeginChild("##store_page_child", ImVec2(child_width, ry), false);
				{
					for (int i = 0; i < (int)api->get_query().size(); i++)
					{
						create_card(api->get_query()[i], i);
					}
				}
				ImGui::EndChild();
			}
			else
			{
				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::Spacing();

				communityPaneDetails();
			}
		}
	}
	void parse_text(const char* text)
	{
		std::istringstream stream(text);
		std::string line;

		const auto has_tag = ([=](std::string& str, const std::string& prefix) {
			bool has = str.substr(0, prefix.size()) == prefix;
			if (has) { str = str.substr(prefix.size()); }
			return has;
			});

		while (std::getline(stream, line, '\n')) {
			if (has_tag(line, "[Separator]")) { ImGui::Separator(); }
			else if (has_tag(line, "[Large]")) {
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]); ImGui::TextWrapped(line.c_str()); ImGui::PopFont();
			}
			else {
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); ImGui::TextWrapped(line.c_str()); ImGui::PopFont();
			}
		}
	}
	void communityPaneDetails()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10);

		auto worksize = ImGui::GetMainViewport()->WorkSize;

		//set constraints on the children
		float child_width = (float)(worksize.x < 800 ? rx : worksize.x < 1400 ? rx / 1.2 : worksize.x < 1800 ? rx / 1.4 : rx / 1.5);

		ui::center(rx, child_width, 8);
		ImGui::BeginChild("store_details", ImVec2(child_width, ry), false);
		{
			ImGui::BeginChild("left_side", ImVec2(rx / 1.5f, ry), true);
			{
				if (ImGui::Button(" < Back "))
				{
					m_Client->b_showingDetails = false;

					//m_Client->releaseImages();
				}


				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				ui::shift::y(10);

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[4]);
				ui::center_text(m_Client->m_resultsSchema.name.c_str());
				ImGui::PopFont();

				ImGui::Spacing();

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.5, .5, .5, 1));

				ui::center_text(std::format("By {} | Downloads {} | ID {}",
					m_Client->m_resultsSchema.gh_username,
					m_Client->m_resultsSchema.download_count,
					m_Client->m_resultsSchema.id
				).c_str());

				ImGui::PopStyleColor();
				ImGui::PopFont();

				ui::shift::y(10);

				ImGui::Separator();

				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::Spacing();


				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

				//get the height of the first element in the array and base the rest of the images height off the first one to prevent image jittering
				float ab_height = (float)image::maintain_aspect_ratio(
					m_Client->v_rawImageList[0].width,
					m_Client->v_rawImageList[0].height, (int)rx, (int)ry
				).height;

				//set max height barrier on the image node
				ab_height = ab_height > (int)ry / 2 - 50 ? (int)ry / 2 - 50 : ab_height;

				//get width and height of the current image
				int height = m_Client->v_rawImageList[m_Client->m_imageIndex].height;
				int width = m_Client->v_rawImageList[m_Client->m_imageIndex].width;

				image::size result = image::maintain_aspect_ratio(width, height, (int)rx, (int)ab_height);

				float parent_width = rx - 30.0f;

				ui::center(rx, parent_width, 10);
				ImGui::BeginChild("image_parent_container", ImVec2(parent_width, (float)(ab_height + 40)), true);
				{
					ui::center(rx, (float)result.width, 1);
					ui::shift::y(20);

					ImGui::BeginChild("image_container", ImVec2((float)result.width, ab_height), true);
					{
						//check if the texture is useable
						if (m_Client->v_rawImageList[m_Client->m_imageIndex].texture != nullptr && m_Client->v_rawImageList[m_Client->m_imageIndex].texture->AddRef())
						{
							ImGui::Image(m_Client->v_rawImageList[m_Client->m_imageIndex].texture, ImVec2((float)result.width, ab_height));
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();

				ImGui::Spacing();

				ui::center(rx, 90.0f, -1);

				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
				ImGui::BeginChild("carousel", ImVec2(90, 20), true);
				{
					if (ImGui::Button(" < ", ImVec2(0, 17))) {
						if (m_Client->m_imageIndex - 1 >= 0) m_Client->m_imageIndex--;
					}
					ImGui::SameLine();
					ImGui::Text(std::format("{} of {}", m_Client->m_imageIndex + 1, m_Client->v_rawImageList.size()).c_str());
					ImGui::SameLine();
					ui::shift::right(20);
					if (ImGui::Button(" > ", ImVec2(0, 17)))
					{
						if (m_Client->m_imageIndex < ((int)m_Client->v_rawImageList.size() - 1))
							m_Client->m_imageIndex++;
					}
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();

				ImGui::PopStyleVar(4);

				ImGui::Spacing();

				parse_text(m_Client->m_resultsSchema.description.c_str());
			}
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("right_side", ImVec2(rx, ry), false);
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

				ImGui::BeginChild("actions_and_about", ImVec2(rx, 325), true);
				{
					ImGui::PopStyleColor();

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					ImGui::Text("Actions");
					ImGui::PopFont();


					static ImU32 col = ImGui::GetColorU32(ImGuiCol_CheckMark);

					bool isInstalled = false;
					bool requiresUpdate = false;

					for (const auto item : m_Client->skinData)
					{
						if (this->m_itemSelectedSource.empty())
							continue;

						if (item.value("source", std::string()) == this->m_itemSelectedSource.value("source", std::string())) {

							//console.log(std::format("local installed: {}", this->m_itemSelectedSource["source"].get<std::string>()));
							isInstalled = true;

							if (item["version"] != this->m_itemSelectedSource["version"])
								requiresUpdate = true;
						}
					}

					if (requiresUpdate || !isInstalled)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, col);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);

						if (m_downloadInProgess)
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						else
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

						if (ImGui::Button((m_downloadInProgess ? m_downloadStatus.c_str() : requiresUpdate ? "Update" : "Download"), ImVec2(rx, 35))) {

							std::cout << m_Client->m_resultsSchema.id << std::endl;

							std::thread([this]() {
								m_downloadInProgess = true;
								api->iterate_download_count(m_Client->m_resultsSchema.id);
								m_Client->m_resultsSchema.download_count++;

								std::cout << m_itemSelectedSource.dump(4) << std::endl;
								this->downloadTheme(m_itemSelectedSource);

								//this->createThemeSync(m_itemSelectedSource);

								m_Client->parseSkinData();
								m_downloadInProgess = false;
							}).detach();
						}

						ImGui::PopStyleColor(2);
						ImGui::PopFont();
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, .5f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.0f, .5f));

						if (ImGui::Button("Uninstall", ImVec2(rx, 35))) {

							std::string disk_path = std::format("{}/{}", config.getSkinDir(), this->sanitizeDirectoryName(m_itemSelectedSource["name"].get<std::string>()));

							console.log(std::format("deleting skin {}", disk_path));

							if (std::filesystem::exists(disk_path)) {

								try {
									std::filesystem::remove_all(std::filesystem::path(disk_path));
								}
								catch (const std::exception& ex) {
									MsgBox(std::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
								}
							}

							m_Client->parseSkinData();
						}

						ImGui::PopStyleColor(2);
					}

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

					if (ImGui::Button("View Source", ImVec2(rx, 35))) {

						OpenURL(std::format("https://github.com/{}/{}", 
							m_Client->m_resultsSchema.gh_username, m_Client->m_resultsSchema.gh_repo).c_str())
					}

					ImGui::PopStyleColor(2);

					ImGui::Spacing();

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					ImGui::Text("About");
					ImGui::PopFont();

					const auto render_about = [=](std::string title, std::string data)
						{
							ImGui::Text(title.c_str());

							ImGui::SameLine();
							ui::shift::x(-6);

							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.5, .5, .5, 1.0f));
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[3]);
							ImGui::Text(data.c_str());
							ImGui::PopFont();
							ImGui::PopStyleColor();
						};

					render_about("Downloads", std::to_string(m_Client->m_resultsSchema.download_count));
					render_about("ID", m_Client->m_resultsSchema.id);
					render_about("Date Added", m_Client->m_resultsSchema.date_added);
					render_about("Uploader", m_Client->m_resultsSchema.gh_username);

					ImGui::Spacing();
					ImGui::Spacing();

					ImGui::BeginChild("author", ImVec2(rx, 45), true);
					{
						ImGui::BeginChild("author_image", ImVec2(25, 25), false);
						{
							ImGui::Image(Window::iconsObj().github, ImVec2(25, 25));
						}
						ImGui::EndChild();
						ImGui::SameLine();
						ImGui::BeginChild("author_desc", ImVec2(rx, ry), false);
						{
							ui::shift::y(-3);
							ImGui::Text(m_Client->m_resultsSchema.gh_username.c_str());
							ui::shift::y(-5);
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
							ImGui::Text("https://github.com/%s", m_Client->m_resultsSchema.gh_username.c_str());
							ImGui::PopFont();
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

				ImGui::BeginChild("support_server", ImVec2(rx, 100), true);
				{

					ImGui::BeginChild("support_image", ImVec2(45, 45), false);
					{
						ImGui::Image(Window::iconsObj().support, ImVec2(45, 45));
					}
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("support_desc", ImVec2(rx, 45), false);
					{
						ui::shift::y(7);
						ImGui::Text(m_Client->m_resultsSchema.discord_name.c_str());
						ui::shift::y(-5);
						ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						ImGui::Text("Support Server");
						ImGui::PopFont();
					}
					ImGui::EndChild();


					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.44f, 0.76f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.36f, 0.62f, 1.0f));

					if (ImGui::Button("Join Server", ImVec2(rx, ry)))
					{
						ShellExecute(NULL, "open", m_Client->m_resultsSchema.discord_link.c_str(), NULL, NULL, SW_SHOWNORMAL);
					}

					ImGui::PopStyleColor(2);
				}
				ImGui::EndChild();

				ImGui::BeginChild("tags-category", ImVec2(rx, 80), true);
				{
					std::vector<std::string> items = m_Client->m_resultsSchema.tags;

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					ImGui::Text("Tags");
					ImGui::PopFont();

					ImGui::BeginChild("support_desc", ImVec2(rx, 30), false);
					{
						ImGui::PopStyleColor();

						ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15);

						for (int i = 0; i < (int)items.size(); i++)
						{
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
							const float width = ImGui::CalcTextSize(items[i].c_str()).x;

							ImGui::BeginChild(std::format("tag_{}", i).c_str(), ImVec2(width + 25.0f, ry), true);
							{
								ui::shift::x(5);
								ImGui::Text(items[i].c_str());
							}
							ImGui::EndChild();

							ImGui::PopFont();

							ImGui::SameLine();
							ui::shift::x(-6);
						}
						ImGui::PopStyleVar();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::PopStyleVar();
	}

	void renderContentPanel()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
		{
			if (ImGui::BeginChild("ChildContentPane", ImVec2(rx, ry), true))
			{
				switch (ui::current_tab_page)
				{
					case 1: store_panel();    break;
					case 2: library_panel();  break;
					case 3: settings_panel(); break;
				}
			}
			ImGui::EndChild();
		}
		ImGui::PopStyleColor();
	}

	void renderSideBar()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.00f));

		ImGui::BeginChild("LeftSideBar", ImVec2(200, ry), false);
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

						//disabled maximize until i figure it out
						ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));

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

				ImGui::BeginChild("ChildHeaderParent", ImVec2(rx, 70), true);
				{
					ImGui::BeginChild("ChildImageContainer", ImVec2(50, 50), false);
					{
						ImGui::Image(Window::iconsObj().planet, ImVec2(50, 50));
					}
					ImGui::EndChild();
					ImGui::SameLine();
					ImGui::BeginChild("ChildDescription", ImVec2(rx, 50), false);
					{
						ui::shift::y(7);
						ImGui::Text("Millennium");
						ui::shift::y(-5);
						ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						{
							ImGui::Text("Steam Client Patcher.");
						}
						ImGui::PopFont();
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();
				ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

				if (ImGui::BeginPopupContextItem("selectable_1")) 
				{
					if (ImGui::MenuItem("Reload")) { api->retrieve_featured(); }
					ImGui::EndPopup();
				}
				if (ImGui::BeginPopupContextItem("selectable_2")) 
				{
					if (ImGui::MenuItem("Reload")) { m_Client->parseSkinData(); }
					ImGui::EndPopup();
				}

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				{
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "MILLENNIUM");
				}
				ImGui::PopFont();

				listButton(" Library", 2);
				listButton(" Community", 1);

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				{
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "GENERAL");
				}
				ImGui::PopFont();

				listButton(" Settings", 3);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

				if (ImGui::Selectable(" About")) {
					ImGui::OpenPopup(" About Millennium");
				}
				ImGui::PopStyleColor();

				//ImGui::InsertNotification({ ImGuiToastType_Error, 3000, "Hello World! This is an error!" });

				//if (ImGui::Button("Unload Millennium")) {
				//	FreeConsole();
				//	MsgBox("Unloading Millennium from Steam", "Error", 0);
				//	
				//	CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, hCurrentModule, NULL, nullptr);

				//	Application::Destroy();
				//	threadContainer::getInstance().killAllThreads(0);
				//}

				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

				ui::center_modal(ImVec2(ImGui::GetMainViewport()->WorkSize.x / 2.3, ImGui::GetMainViewport()->WorkSize.y / 2.4));

				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);

				if (ImGui::BeginPopupModal(" About Millennium", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
				{
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));

					ImGui::BeginChild("HeaderOptions", ImVec2(rx, 25));
					{
						if (ImGui::IsWindowHovered())
						{
							ImGui::Image(Window::iconsObj().close, ImVec2(11, 11));
							if (ImGui::IsItemClicked()) {
								ImGui::CloseCurrentPopup();
							}
						}
						else
						{
							ImGui::Image(Window::iconsObj().greyed_out, ImVec2(11, 11));
						}

						ImGui::SameLine();

						ImGui::BeginChild("HeaderDragArea", ImVec2(rx, ry));
						{
							ImGui::PopStyleColor();

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

					ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
					{
						ui::center_text("About Millennium");
					}
					ImGui::PopFont();
					ImGui::Spacing();

					ui::center_text(std::format("Build: {}", __DATE__).c_str());
					ui::center_text(std::format("Millennium API Version: {}", "v1").c_str());
					ui::center_text(std::format("Client Version: {}", "1.0.0").c_str());
					ui::center_text(std::format("Patcher Version: {}", m_ver).c_str());

					ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - 100);
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

					ImGui::BeginChild("support_server", ImVec2(rx, 100), true);
					{
						ImGui::BeginChild("support_image", ImVec2(45, 45), false);
						{
							ImGui::Image(Window::iconsObj().support, ImVec2(45, 45));
						}
						ImGui::EndChild();

						ImGui::SameLine();

						ImGui::BeginChild("support_desc", ImVec2(rx, 45), false);
						{
							ui::shift::y(7);
							ImGui::Text("Millennium");
							ui::shift::y(-5);
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
							ImGui::Text("Need Help? Found a bug?");
							ImGui::PopFont();
						}
						ImGui::EndChild();


						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.44f, 0.76f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.36f, 0.62f, 1.0f));

						if (ImGui::Button("Join Server", ImVec2(rx, ry)))
						{
							ShellExecute(NULL, "open", "https://discord.gg/MXMWEQKgJF", NULL, NULL, SW_SHOWNORMAL);
						}

						ImGui::PopStyleColor(2);
					}
					ImGui::EndChild();

					ImGui::PopStyleColor();

					ImGui::EndPopup();
				}

				ImGui::PopStyleVar();

				//ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				//{
				//	std::string v_buildInfo = std::format("Build: {}\nMillennium Patcher Version: {}", __DATE__, m_ver);
				//	ImGui::SetCursorPosY(ImGui::GetContentRegionMax().y - ImGui::CalcTextSize(v_buildInfo.c_str()).y);
				//	ImGui::Text(v_buildInfo.c_str());
				//}
				//ImGui::PopFont();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
	}
} RendererProc;

void handleFileDrop()
{
	if (g_fileDropQueried) {
		ImGui::OpenPopup("###FileDropPopup");
	}

	ui::center_modal(ImVec2(ImGui::GetMainViewport()->WorkSize.x / 2.3, ImGui::GetMainViewport()->WorkSize.y / 2.4));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);

	ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

	if (ImGui::BeginPopupModal("###FileDropPopup", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

		
		ImGui::PopStyleColor();

		if (!g_fileDropQueried) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();
}

void handleEdit()
{
	const std::string name = RendererProc.m_editObj.empty() ? "" : (RendererProc.m_editObj.contains("name") ? RendererProc.m_editObj["name"] : RendererProc.m_editObj["native-name"]);


	if (RendererProc.m_editMenuOpen) {
		ImGui::OpenPopup(std::format(" Settings for {}", name).c_str());
	}

	ui::center_modal(ImVec2(ImGui::GetMainViewport()->WorkSize.x / 2, ImGui::GetMainViewport()->WorkSize.y / 1.5));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6);

	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));


	if (ImGui::BeginPopupModal(std::format(" Settings for {}", name).c_str(), &RendererProc.m_editMenuOpen, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		ImGui::BeginChild("###ConfigContainer", ImVec2(rx, ry - 32), false);
		{
			if (RendererProc.m_editObj.contains("Configuration"))
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
					{
						continue;
					}
					if (!setting.contains("Type"))
					{
						continue;
					}

					const std::string name = setting["Name"];
					const std::string toolTip = setting.value("ToolTip", std::string());
					const std::string type = setting["Type"];



					if (type == "CheckBox")
					{
						bool value = setting.value("Value", false);

						ui::render_setting(
							name.c_str(), toolTip.c_str(),
							value, false,
							[&]() {
								std::cout << "callback" << std::endl;

								setting["Value"] = value;
							}
						);
					}
				}
			}

			if (!RendererProc.colorList.empty())
			{
				ImGui::Spacing();
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
				ImGui::Text(" Color Settings:");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::PopFont();

				auto& colors = RendererProc.colorList;

				for (int i = 0; i < RendererProc.colorList.size(); i++)
				{
					ImGui::Text(std::format("{} [{}]", colors[i].comment, colors[i].name).c_str());

					if (ImGui::ColorEdit3(std::format("##colorpicker_{}", i).c_str(), &colors[i].color.x), ImGuiColorEditFlags_NoLabel) {
						// The user has selected a color; 'color' now contains the selected color.
						// You can choose to ignore the alpha channel if needed.
					}
				}

				if (ImGui::Button("Reset Colors"))
				{
					auto& obj = RendererProc.m_editObj;

					if (obj.contains("source") && obj.contains("GlobalsColors"))
					{
						const auto original = http::getJson(std::format("{}/{}",
							obj["source"].get<std::string>(), obj["GlobalsColors"].get<std::string>()));

						if (original.contains("message"))
						{
							MsgBox(std::format("Can't GET the original colors from source.\nMessage:\n{}", original["message"].get<std::string>()).c_str(), "Error", MB_ICONERROR);
						}
						else
						{
							console.log(original.dump(4));

							if (original.contains("download_url"))
							{
								const auto fileContents = http::get(original["download_url"].get<std::string>());

								std::vector<unsigned char> byteVector;

								for (char c : fileContents) {
									byteVector.push_back(static_cast<unsigned char>(c));
								}

								RendererProc.writeFileBytesSync(RendererProc.colorFilePathBuffer, byteVector);

								console.log("Wrote original file buffer back to: " + RendererProc.colorFilePathBuffer);
							}
							else
							{
								MsgBox("Can't GET the original colors from source as the API endpoint doesn't include a download_url", "Error", MB_ICONERROR);
							}
						}
					}
					else {
						console.err("Can't retrieve original colors because the selected skin doesn't have 'source' key or 'GlobalsColors'");
					}
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (ImGui::Button("Save and Update", ImVec2(rx, 24)))
		{
			const auto json = RendererProc.m_editObj;

			std::vector<unsigned char> byteVector;

			nlohmann::json buffer = config.getThemeData(true);

			if (buffer.contains("config_fail")) {
				MsgBox("Unable to save and update config. Millennium couldn't get theme data", "Error", MB_ICONERROR);
			}
			else {
				buffer["Configuration"] = json["Configuration"];

				for (char c : buffer.dump(4)) {
					byteVector.push_back(static_cast<unsigned char>(c));
				}

				std::cout << buffer.dump(4) << std::endl;

				RendererProc.writeFileBytesSync(
					std::filesystem::path(config.getSkinDir()) / json["native-name"] / "skin.json", byteVector
				);

				auto& colors = RendererProc.colorList;

				for (auto color : colors)
				{
					RendererProc.updateColor(RendererProc.colorFileBuffer, color.name, ImVec4ToHex(color.color));

					std::vector<unsigned char> byteVector;

					for (char c : RendererProc.colorFileBuffer) {
						byteVector.push_back(static_cast<unsigned char>(c));
					}

					RendererProc.writeFileBytesSync(RendererProc.colorFilePathBuffer, byteVector);
				}

				themeConfig::updateEvents::getInstance().triggerUpdate();

				steam_js_context SharedJsContext;
				SharedJsContext.reload();
			}
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();
}

void init_main_window()
{
	// GET information on initial load
	const auto initCallback = ([=](void) -> void {
		m_Client->parseSkinData();
		api->retrieve_featured();
	});

	// window callback 
	const auto wndProcCallback = ([=](void) -> void {
		RendererProc.renderSideBar();
		ImGui::SameLine(); ui::shift::x(-10);
		RendererProc.renderContentPanel();

		handleFileDrop();
		handleEdit();
	});

	std::thread([&]() {
		themeConfig::watchPath(config.getSkinDir(), []() {
			try {
				//m_Client->parseSkinData();
			}
			catch (std::exception& ex) {
				console.log(ex.what());
			}
		});
	}).detach();

	Window::setTitle((char*)"Millennium.Steam.Client");
	Window::setDimensions(ImVec2({ 1050, 800 }));

	Application::Create(wndProcCallback, initCallback);
}
