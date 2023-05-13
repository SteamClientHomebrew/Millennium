#pragma once

#include <d3d9.h>
#include <vendor/imgui/imgui.h>
#include <vendor/imgui/imgui_impl_dx9.h>
#include <vendor/imgui/imgui_impl_win32.h>

#include <D3dx9tex.h>
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "d3d9.lib")

class WindowClass
{
public:
    static void WindowTitle(char* name);
    static void WindowDimensions(ImVec2 dimensions);
    static void StartApplication(std::string path);
};

void CenterModal(ImVec2 size);

//GUI helpers
void Center(float avail_width, float element_width, float padding = 15);
void Center_Text(const char* format, float spacing = 15, ImColor color = ImColor(255, 255, 255));

//Process handler, ie window destroying, changing size and other commands
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Application
{
public:
    static bool Create(std::string& m_window_title, void (*Handler)(void));
    ~Application(); // TODO
};

