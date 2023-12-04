#pragma once

#include <d3d9.h>
#include <src/vendor/imgui/imgui.h>
#include <src/vendor/imgui/imgui_impl_dx9.h>
#include <src/vendor/imgui/imgui_impl_win32.h>

//#include <D3dx9tex.h>
#include <src/vendor/imgui/imgui_internal.h>
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "d3d9.lib")

#include <functional>
#include <format>

extern bool app_open;
extern bool g_headerHovered;
extern bool g_headerHovered_1;
extern ImVec2 g_windowPadding;

#define rx ImGui::GetContentRegionAvail().x
#define ry ImGui::GetContentRegionAvail().y


struct icons
{
    PDIRECT3DTEXTURE9
        close,
        minimize,
        maximize,
        exit,
        greyed_out,
        planet;
};


class Window
{
public:
    static void setTitle(char* name);
    static void setDimensions(ImVec2 dimensions);

    static LPDIRECT3DDEVICE9* getDevice();
    static icons iconsObj();


    static HWND getHWND();

    static void bringToFront();

    static const void center_modal(ImVec2 size);
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


//Process handler, ie window destroying, changing size and other commands
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Application
{
public:
    static bool Create(std::function<void()> Handler, HINSTANCE hInstance);
    static bool Destroy();
};


/// <summary>
/// extend the ImGui namespace with a spinner function
/// </summary>
namespace ImGui {
    inline bool Spinner(const char* label, float radius, int thickness, const ImU32& color) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size((radius) * 2, (radius + style.FramePadding.y) * 2);

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ItemSize(bb, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        // Render
        window->DrawList->PathClear();

        int num_segments = 30;
        int start = (int)abs(ImSin((float)g.Time * 1.8f) * (num_segments - 5.0f));

        const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
        const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

        const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

        for (int i = 0; i < num_segments; i++) {
            const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + (float)g.Time * 8.0f) * radius,
                centre.y + ImSin(a + (float)g.Time * 8.0f) * radius));
        }

        window->DrawList->PathStroke(color, false, (float)thickness);
    }
}