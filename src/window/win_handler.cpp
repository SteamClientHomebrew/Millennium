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
	const void writeFileSync(const std::filesystem::path& filePath, const std::string& fileContent)
	{
		std::ofstream fileStream(filePath);
		fileStream << fileContent;
		fileStream.close();
	}
	std::string sanitizeDirectoryName(const std::string& input) {
		// Regular expression to match characters that are not URL-encoded
		std::regex invalidCharRegex("[^a-zA-Z0-9-_.~]");
		// Replace invalid characters with an empty string
		return std::regex_replace(input, invalidCharRegex, "");
	}
	std::vector<std::string> queryThemeData(nlohmann::basic_json<>& skinData)
	{
		std::vector<std::string> fileItems;
		std::set<std::string> processedNames;

		for (const auto& patch : skinData["Patches"])
		{
			if (patch.contains("TargetCss"))
			{
				const auto styleSheet = patch["TargetCss"].get<std::string>();

				if (processedNames.find(styleSheet) != processedNames.end()) {
					continue;
				}

				fileItems.push_back(styleSheet);
				processedNames.insert(styleSheet);
			}
			if (patch.contains("TargetJs"))
			{
				const auto javaScript = patch["TargetJs"].get<std::string>();

				if (processedNames.find(javaScript) != processedNames.end()) {
					continue;
				}

				fileItems.push_back(javaScript);
				processedNames.insert(javaScript);
			}
		}

		for (const auto& hook : skinData["Hooks"])
		{
			if (hook.contains("Interpolate"))
			{
				const auto interpFile = hook["Interpolate"].get<std::string>();

				if (processedNames.find(interpFile) != processedNames.end()) {
					continue;
				}

				fileItems.push_back(interpFile);
				processedNames.insert(interpFile);
			}
		}

		return fileItems;
	}
	const void findImportsSync(const std::string& content, std::filesystem::path& githubUrl, std::filesystem::path& parentPath)
	{
		using namespace std::filesystem;

		std::smatch matches;
		std::string::const_iterator searchStart(content.cbegin());

		std::regex importRegex("@import\\s+url\\(([^\)]+)\\);");

		while (std::regex_search(searchStart, content.cend(), matches, importRegex))
		{
			if (matches.size() <= 1) {
				continue;
			}

			const auto importedFileName = matches[1].str();
			console.log(std::format("# Import: {}", importedFileName));
			m_downloadStatus = std::format("GET {}", importedFileName);

			path importUrl = githubUrl / parentPath / importedFileName;
			path resolvedPath = parentPath / importedFileName;

			try {
				if (resolvedPath.has_parent_path()) {
					create_directories(resolvedPath.parent_path());
				}
				this->writeFileSync(resolvedPath, http::get(importUrl.string()));
			}
			catch (const http_error& error)
			{
				switch (error.code())
				{
				case http_error::errors::couldnt_connect: {
					MsgBox(std::format("A file couldn't be downloaded {}", importUrl.string()).c_str(), "Millennium", MB_ICONERROR); break;
				}
				default: { MsgBox("The selected theme seems to be malformed", "Error", MB_ICONERROR); }
				}
			}
			searchStart = matches.suffix().first;
		}
	}
	const void createThemeSync(nlohmann::basic_json<> skinData)
	{
		using namespace std::filesystem;

		path githubUrl = skinData["source"].get<std::string>();
		path dirName   = path("steamui") / "skins" / this->sanitizeDirectoryName(skinData["name"].get<std::string>());

		path originalDir = current_path();
		create_directory(dirName);
		current_path(dirName);

		this->writeFileSync("skin.json", skinData.dump(4));

		std::vector<std::string> fileItems = this->queryThemeData(skinData);

		for (const auto& fileName : fileItems)
		{
			path parentPath;
			path relativeUrlPath = githubUrl / fileName;

			const auto p_fileName = path(fileName);
			if (p_fileName.has_parent_path())
			{
				create_directories(p_fileName.parent_path());
				parentPath = p_fileName.parent_path();
			}

			console.log(std::format("GET {}", fileName));
			m_downloadStatus = std::format("GET {}", fileName);

			const auto content = http::get(relativeUrlPath.string());

			this->findImportsSync(content, githubUrl, parentPath);

			this->writeFileSync(fileName, content);
		}

		current_path(originalDir);
	}
public:

	void createLibraryListing(nlohmann::basic_json<>& skin, int index)
	{
		const std::string m_skinName = skin["native-name"];

		static bool push_popped = false;
		static int hovering;

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


		if (m_Client->m_currentSkin == m_skinName)
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_CheckMark));
		else
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 0.0f));

		ImGui::BeginChild(std::format("card_child_container_{}", index).c_str(), ImVec2(rx, 50 + desc_height), true, ImGuiWindowFlags_ChildWindow);
		{
			//reset the window padding rules
			ImGui::PopStyleVar();
			popped = true;

			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6);

			ImGui::PopStyleColor();

			//display the title bar of the current skin 
			ImGui::BeginChild(std::format("skin_header{}", index).c_str(), ImVec2(rx, 33), true);
			{
				if (skin["remote"])
				{
					ImGui::Image((void*)Window::iconsObj().icon_remote_skin, ImVec2(ry - 1, ry - 1));
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
					ImGui::Image((void*)Window::iconsObj().skin_icon, ImVec2(ry - 1, ry - 1));

				ImGui::SameLine();

				ui::shift::x(-4);

				ImGui::Text(skin.value("name", "null").c_str());

				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - 5, ImGui::GetCursorPosY() + 1));

				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), std::format("{} by {}", skin.value("version", "1.0.0"), skin.value("author", "unknown")).c_str());

				ImGui::PopFont();
			}
			ImGui::EndChild();
			ImGui::PopStyleVar();

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);

			ImGui::BeginChild("###description_child", ImVec2(rx - 45, ry), false);
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.0));
				ImGui::TextWrapped(skin.value("description", "null").c_str());
				ImGui::PopStyleColor();
			}
			ImGui::EndChild();
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
				std::string remote_skin = std::format("{}/{}", config.getSkinDir(), skin["native-name"].get<std::string>());
				std::string disk_path = std::format("{}/{}/skin.json", config.getSkinDir(), skin["native-name"].get<std::string>());

				if (std::filesystem::exists(disk_path)) {
					std::filesystem::remove_all(std::filesystem::path(disk_path).parent_path());
				}
				if (std::filesystem::exists(remote_skin)) {
					std::filesystem::remove(remote_skin);
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
			hovering = index;
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

		ImGui::BeginChild("###library_container", ImVec2(child_width, ry), false);
		{
			static const auto to_lwr = [](std::string str) {
				std::string result;
				for (char c : str) {
					result += std::tolower(c);
				}
				return result;
			};

			for (size_t i = 0; i < m_Client->skinData.size(); ++i)
			{
				nlohmann::basic_json<>& skin = m_Client->skinData[i];

				if (skin.value("native-name", std::string()) == m_Client->m_currentSkin)
				{
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

			ui::shift::right(115); ui::shift::y(-3);

			static int p_sortMethod = 0;

			static const char* items[] = { "All Items", "Installed", "Cloud" };

			ImGui::PushItemWidth(115);
			if (ImGui::Combo(std::format("Sort").c_str(), &p_sortMethod, items, IM_ARRAYSIZE(items)));
			ImGui::PopItemWidth();

			if (m_Client->skinData.empty()) {
				ui::shift::y(150);

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
		m_Client->m_resultsSchema = item;
		m_Client->getRawImages(item.v_images);
		m_Client->b_showingDetails = true;
		m_Client->m_imageIndex = 0;

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

				skin_details_panel();
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
	void skin_details_panel()
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


					bool installed = false;
					bool local_installed = false;

					//nlohmann::json skin = {
					//	{"gh_username", m_Client->m_resultsSchema.gh_username},
					//	{"gh_repo", m_Client->m_resultsSchema.gh_repo},
					//	{"skin-json", m_Client->m_resultsSchema.skin_json}
					//};

					//for (const auto _item : m_Client->remoteSkinData)
					//{
					//	std::cout << _item.dump(4) << std::endl;

					//	if (_item == skin)
					//		installed = true;
					//}

					for (const auto item : m_Client->skinData)
					{
						if (this->m_itemSelectedSource.empty())
							continue;


						if (item.value("source", std::string()) == this->m_itemSelectedSource["source"]) {
							local_installed = true;
						}
					}

					//if (installed)
					//{
					//	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, .5f));
					//	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.0f, .5f));

					//	if (ImGui::Button("Remove", ImVec2(rx, 35))) {

					//		std::cout << skin.dump(4) << std::endl;

					//		m_Client->dropLibraryItem(m_Client->m_resultsSchema, skin);
					//		m_Client->parseSkinData();
					//	}
					//}
					//else
					//{
					//	ImGui::PushStyleColor(ImGuiCol_Button, col);
					//	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);

					//	if (ImGui::Button("Download", ImVec2(rx, 35))) {

					//		std::cout << m_Client->m_resultsSchema.id << std::endl;

					//		api->iterate_download_count(m_Client->m_resultsSchema.id);

					//		m_Client->m_resultsSchema.download_count++;

					//		m_Client->concatLibraryItem(m_Client->m_resultsSchema, skin);
					//		m_Client->parseSkinData();
					//	}
					//}

					if (!local_installed)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, col);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);

						//std::cout << (installed ? "true" : "false") << std::endl;

						if (m_downloadInProgess)
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
						else
							ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

						if (ImGui::Button((m_downloadInProgess ? m_downloadStatus.c_str() : "Download"), ImVec2(rx, 35))) {

							std::cout << m_Client->m_resultsSchema.id << std::endl;

							std::thread([this]() {
								m_downloadInProgess = true;
								api->iterate_download_count(m_Client->m_resultsSchema.id);
								m_Client->m_resultsSchema.download_count++;

								this->createThemeSync(m_itemSelectedSource);

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

							std::string disk_path = std::format("{}/{}/skin.json", config.getSkinDir(), this->sanitizeDirectoryName(m_itemSelectedSource["name"].get<std::string>()));

							if (std::filesystem::exists(disk_path)) {
								std::filesystem::remove_all(std::filesystem::path(disk_path).parent_path());
							}

							m_Client->parseSkinData();
						}

						ImGui::PopStyleColor(2);
					}

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

					//if (installed)
					//{
					//	if (ImGui::Button("Add to Library", ImVec2(rx, 35)))
					//	{
					//		std::cout << m_Client->m_resultsSchema.id << std::endl;

					//		api->iterate_download_count(m_Client->m_resultsSchema.id);

					//		m_Client->m_resultsSchema.download_count++;

					//		m_Client->concatLibraryItem(m_Client->m_resultsSchema, skin);
					//		m_Client->parseSkinData();
					//	}
					//	if (ImGui::IsItemHovered())
					//	{
					//		ImGui::SetTooltip(
					//			"Link the cloud version of the skin to your library.\n"
					//			"This means updates to the skin will be automatically be added to your client when they come.\n"
					//			"However locally installing the theme will perform better and the theme will also work offline.\n\n"
					//			"Keep in mind that as new updates come, if there is malicious code added to the skin you may not notice it\n"
					//			"if you use the cloud version.\n\n"
					//			"Only use cloud skins from millennium developers or other developers you trust."
					//		);
					//	}
					//}
					//else
					//{
					//	if (ImGui::Button("Remove from Library", ImVec2(rx, 35))) {
					//		m_Client->dropLibraryItem(m_Client->m_resultsSchema, skin);
					//		m_Client->parseSkinData();
					//	}
					//}

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
	});

	std::thread([&]() {
		themeConfig::watchPath(config.getSkinDir(), []() { m_Client->parseSkinData(); });
	}).detach();

	Window::setTitle((char*)"Millennium.Steam.Client");
	Window::setDimensions(ImVec2({ 1050, 800 }));

	Application::Create(wndProcCallback, initCallback);
}
