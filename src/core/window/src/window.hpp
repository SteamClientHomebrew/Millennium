#pragma once

#include <d3d9.h>
#include <core/window/imgui/imgui.h>
#include <core/window/imgui/imgui_impl_dx9.h>
#include <core/window/imgui/imgui_impl_win32.h>

//#include <D3dx9tex.h>
#include <core/window/imgui/imgui_internal.h>
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "d3d9.lib")

extern bool app_open;

struct icons
{
    PDIRECT3DTEXTURE9 icon;

    PDIRECT3DTEXTURE9 store, library, settings, trash_icon, skin_icon, reload_icon, icon_no_results, icon_remote_skin, more_icon, icon_saved;
    PDIRECT3DTEXTURE9 check_mark_checked, check_mark_unchecked;

    PDIRECT3DTEXTURE9 close, minimize, exit;
};

class window_class
{
public:
    static void wnd_title(char* name);
    static void wnd_dimension(ImVec2 dimensions);

    static LPDIRECT3DDEVICE9 device();
    static icons get_ctx();

    static HWND get_hwnd();

    static void wnd_close();
    static void wnd_minmize();
};

//Process handler, ie window destroying, changing size and other commands
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Application
{
public:
    static bool Create(std::string& m_window_title, void (*Handler)(void));
};