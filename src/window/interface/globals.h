#pragma once
#include <stdafx.h>
//
//#include <imgui.h>
//#include <imgui_internal.h>

#include <utils/config/config.hpp>
#include <core/steam/cef_manager.hpp>

#include <nlohmann/json.hpp>
#include <filesystem>

//macros for getting available [x, y]
#define rx ImGui::GetContentRegionAvail().x
#define ry ImGui::GetContentRegionAvail().y

enum tabPages
{
	storeTab = 1,
	libraryTab = 2,
	settingsTab = 3
};

struct ComboBoxItem
{
	std::string name;
	int value;
};

static std::vector<ComboBoxItem> comboBoxItems;

inline bool comboAlreadyExists(const std::string& name)
{
	for (const auto& item : comboBoxItems)
	{
		if (item.name == name)
			return true;
	}
	return false;
}

inline int getComboValue(const std::string name)
{
	for (const auto& item : comboBoxItems)
	{
		if (item.name == name)
			return item.value;
	}
	return -1;
}

inline void setComboValue(const std::string name, int value)
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
//
//namespace ui
//{
//	static int current_tab_page = 2;
//
//	namespace shift
//	{
//		static void x(int value) {
//			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + value);
//		}
//		static void y(int value) {
//			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + value);
//		}
//		static void right(int item_width) {
//			ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - item_width);
//		}
//	}
//
//	static const void render_setting(const char* name, const char* description, bool& setting, bool requires_restart, std::function<void()> call_back) {
//		ImGui::Text(name);
//
//		ImGui::SameLine();
//
//		ui::shift::x(-3);
//		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
//		ImGui::TextWrapped("(?)");
//		ImGui::PopStyleColor();
//
//		if (ImGui::IsItemHovered()) {
//			ImGui::SetTooltip(description);
//		}
//
//		ImGui::SameLine();
//
//		ui::shift::right(25); ui::shift::y(-3);
//		if (ImGui::Checkbox(fmt::format("###{}", name).c_str(), &setting)) call_back();
//
//		if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
//	}
//
//	static const void render_setting_list(const char* name, const char* description, int& setting, const char* items[], size_t items_size, bool requires_restart, std::function<void()> call_back) {
//		ImGui::Text(name);
//
//		ImGui::SameLine();
//
//		ui::shift::x(-3);
//		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
//		ImGui::TextWrapped("(?)");
//		ImGui::PopStyleColor();
//
//		if (ImGui::IsItemHovered()) {
//			ImGui::SetTooltip(description);
//		}
//
//		ImGui::SameLine();
//
//		int width = (int)ImGui::CalcTextSize(items[setting]).x + 50;
//
//		ui::shift::right(width); ui::shift::y(-3);
//
//		ImGui::PushItemWidth((float)width);
//		if (ImGui::Combo(fmt::format("###{}", name).c_str(), &setting, items, items_size)) call_back();
//		ImGui::PopItemWidth();
//
//		if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
//	}
//
//	static const void center_modal(ImVec2 size) {
//		//RECT rect;
//		//GetWindowRect(Window::getHWND(), &rect);
//
//		//float xPos = rect.left + (rect.right - rect.left - size.x) / 2;
//		//float yPos = rect.top + (rect.bottom - rect.top - size.y) / 2;
//
//		//ImGui::SetNextWindowPos(ImVec2(xPos, yPos));
//		//ImGui::SetNextWindowSize(size);
//	}
//
//	static const void center(float, float element_width, float) {
//		//take available region and the element width and subtract to there, and then account for framepadding causing displacement
//		ImGui::SetCursorPosX((rx - element_width) / 2 + (ImGui::GetStyle().FramePadding.x * 2));
//	}
//
//	static const void center_text(const char* format) {
//		center(0, ImGui::CalcTextSize(format).x, 0);
//		ImGui::Text(format);
//	}
//}

class millennium
{
private:
	void resetCollected();
	void parseRemoteSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer);

public:

	std::vector<nlohmann::basic_json<>> add_update_status_to_client(
		std::vector<nlohmann::basic_json<>>& buffer, nlohmann::json nativeName, bool needsUpdate);

	nlohmann::json get_versions_from_disk(std::vector<nlohmann::basic_json<>>& buffer);

	std::vector<nlohmann::basic_json<>> get_update_list(
		std::vector<nlohmann::basic_json<>>& buffer,
		bool reset_version,
		std::string reset_name
	);

	std::vector<nlohmann::basic_json<>> use_local_update_cache(std::vector<nlohmann::basic_json<>>& buffer);

	nlohmann::basic_json<> readFileSync(std::string path);

	bool skinDataReady;

	bool b_javascriptEnabled;
	bool b_networkingEnabled;
	std::string m_currentSkin;

	std::vector<nlohmann::basic_json<>>
		skinData, remoteSkinData;

	// no internet and cant connect to retrieve a remote skin
	int m_missingSkins = 0;
	// skins that are invalid like a remote skin that was attempted to be created with networking disabled
	int m_unallowedSkins = 0;

	//index of the showing image on the details page
	int m_imageIndex = 0;

	//MillenniumAPI::resultsSchema m_resultsSchema;

	bool b_noConnection = false;
	bool b_showingDetails = false;
	//std::vector<image::image_t> v_rawImageList;

	void getRawImages(std::vector<std::string>& images);
	void parseSkinData(bool checkForUpdates = false, bool setCommit = false, std::string newCommit = "");
	nlohmann::basic_json<> bufferSkinData();
	void changeSkin(nlohmann::basic_json<>& skin);
	//void concatLibraryItem(MillenniumAPI::resultsSchema item, nlohmann::json& skin);
	//void dropLibraryItem(MillenniumAPI::resultsSchema item, nlohmann::json& skin);

	bool isInvalidSkin(std::string skin = Settings::Get<std::string>("active-skin"));

	bool parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer, bool checkForUpdates = false);
	void releaseImages();

	~millennium() noexcept
	{
		//for (auto& item : v_rawImageList) {
		//	if (item.texture) item.texture->Release();
		//}
		//v_rawImageList.clear();
	}
};

static millennium m_Client;

