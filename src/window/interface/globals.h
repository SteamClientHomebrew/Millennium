#pragma once
#include <stdafx.h>

#include <window/imgui/imgui.h>
#include <window/imgui/imgui_internal.h>

#include <window/core/window.hpp>
#include <window/api/api.hpp>
#include <utils/config/config.hpp>
#include <core/steam/cef_manager.hpp>

//macros for getting available [x, y]
#define rx ImGui::GetContentRegionAvail().x
#define ry ImGui::GetContentRegionAvail().y

enum tabPages
{
	storeTab = 1,
	libraryTab = 2,
	settingsTab = 3
};

namespace ui
{
	static int current_tab_page = 2;

	namespace shift
	{
		static void x(int value) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + value);
		}
		static void y(int value) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + value);
		}
		static void right(int item_width) {
			ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - item_width);
		}
	}

	static const void render_setting(const char* name, const char* description, bool& setting, bool requires_restart, std::function<void()> call_back) {
		ImGui::Text(name); ImGui::SameLine(); ImGui::SameLine();
		ui::shift::right(15); ui::shift::y(-3);
		if (ImGui::Checkbox(std::format("###{}", name).c_str(), &setting)) call_back();

		if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::TextWrapped(description);
		ImGui::PopStyleColor();
	}

	static const void render_setting_list(const char* name, const char* description, int& setting, const char* items[], size_t items_size, bool requires_restart, std::function<void()> call_back) {
		ImGui::Text(name); ImGui::SameLine(); ImGui::SameLine();
		ui::shift::right(115); ui::shift::y(-3);

		ImGui::PushItemWidth(115);
		if (ImGui::Combo(std::format("###{}", name).c_str(), &setting, items, items_size)) call_back();
		ImGui::PopItemWidth();

		if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::TextWrapped(description);
		ImGui::PopStyleColor();
	}

	static const void center_modal(ImVec2 size) {
		RECT rect;
		GetWindowRect(Window::getHWND(), &rect);

		float xPos = rect.left + (rect.right - rect.left - size.x) / 2;
		float yPos = rect.top + (rect.bottom - rect.top - size.y) / 2;

		ImGui::SetNextWindowPos(ImVec2(xPos, yPos));
		ImGui::SetNextWindowSize(size);
	}

	static const void center(float, float element_width, float) {
		//take available region and the element width and subtract to there, and then account for framepadding causing displacement
		ImGui::SetCursorPosX((rx - element_width) / 2 + (ImGui::GetStyle().FramePadding.x * 2));
	}

	static const void center_text(const char* format) {
		center(0, ImGui::CalcTextSize(format).x, 0);
		ImGui::Text(format);
	}
}

class millennium
{
private:
	nlohmann::basic_json<> readFileSync(std::string path);

	bool isInvalidSkin(std::string skin = Settings::Get<std::string>("active-skin"));
	void resetCollected();
	void parseRemoteSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer);
	bool parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer);

public:
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

	MillenniumAPI::resultsSchema m_resultsSchema;

	bool b_noConnection = false;
	bool b_showingDetails = false;
	std::vector<image::image_t> v_rawImageList;

	void getRawImages(std::vector<std::string>& images);
	void parseSkinData();
	void changeSkin(nlohmann::basic_json<>& skin);
	void concatLibraryItem(MillenniumAPI::resultsSchema item, nlohmann::json& skin);
	void dropLibraryItem(MillenniumAPI::resultsSchema item, nlohmann::json& skin);

	void releaseImages();

	~millennium() noexcept
	{
		for (auto& item : v_rawImageList) {
			if (item.texture) item.texture->Release();
		}
		v_rawImageList.clear();
	}
};

static millennium* m_Client = new millennium;

