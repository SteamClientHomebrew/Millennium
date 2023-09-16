#pragma once

#include <d3d9.h>
#include <window/imgui/imgui.h>
#include <window/imgui/imgui_impl_dx9.h>
#include <window/imgui/imgui_impl_win32.h>

//#include <D3dx9tex.h>
#include <window/imgui/imgui_internal.h>
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "d3d9.lib")

extern bool app_open;
extern bool g_windowOpen;
extern bool g_headerHovered;
extern bool g_headerHovered_1;
extern ImVec2 g_windowPadding;

extern bool g_initReady;

struct icons
{
    PDIRECT3DTEXTURE9 

    icon, 
    store, 
    library, 
    settings, 
    trash_icon, 
    skin_icon,
    reload_icon, 
    icon_no_results, 
    icon_remote_skin, 
    more_icon,
    icon_saved, 
    download, 
    check_mark_checked, 
    check_mark_unchecked, 
    close, 
    minimize, 
    maximize, 
    exit,
    github, 
    support,
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
};

//Process handler, ie window destroying, changing size and other commands
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Application
{
    public: 
        static bool Create(std::function<void()> Handler, std::function<void()> init_call_back);
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