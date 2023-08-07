#include <stdafx.h>
#include <Windows.h>
#include <TlHelp32.h>
#include <Shlwapi.h>

#include <core/window/src/window.hpp>

#include <core/window/imgui/imgui.h>
#include <core/window/imgui/imgui_internal.h>
#include <vector>

#include <strsafe.h>
#include <core/window/src/memory.h>

#include <d3d9.h>
#include <d3dx9tex.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

static bool mousedown = false;
bool app_open = true;

icons icons_struct = {};
icons window_class::get_ctx() { return icons_struct; }

void set_proc_theme_colors();

struct dx9_interface  { LPDIRECT3D9 ID3D9; LPDIRECT3DDEVICE9 device; D3DPRESENT_PARAMETERS params; MSG msg; } directx9;
struct overlay_window { WNDCLASSEX wndex; HWND hwnd; LPCSTR name; } overlay;
struct app_ctx        { std::string Title; uint32_t width = 1215, height = 850; } appinfo;

void window_class::wnd_title(char* name) { appinfo.Title = name; }
void window_class::wnd_dimension(ImVec2 dimensions) { appinfo.width = dimensions.x; appinfo.height = dimensions.y; }

HWND window_class::get_hwnd()    { return overlay.hwnd; }
void window_class::wnd_close()   { PostMessage(overlay.hwnd, WM_CLOSE, 0, 0); mousedown = false; }
void window_class::wnd_minmize() { ShowWindow(overlay.hwnd, SW_MINIMIZE);     mousedown = false; }

LPDIRECT3DDEVICE9 window_class::device() { return directx9.device; }

bool create_device_d3d(HWND hWnd) 
{
    if ((directx9.ID3D9 = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) 
    {
        return false;
    }
    ZeroMemory(&directx9.params, sizeof(directx9.params));

    directx9.params.Windowed               = TRUE;
    directx9.params.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    directx9.params.BackBufferFormat       = D3DFMT_UNKNOWN;
    directx9.params.EnableAutoDepthStencil = TRUE;
    directx9.params.AutoDepthStencilFormat = D3DFMT_D16;
    directx9.params.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
    
    if (directx9.ID3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &directx9.params, &directx9.device) < 0) 
    {
        return false;
    }
    return true;
}
 
void clear_all()
{
    if (directx9.device) { directx9.device->Release(); directx9.device = nullptr; }
    if (directx9.ID3D9)  { directx9.ID3D9->Release();  directx9.ID3D9 = nullptr;  }

    UnregisterClass(overlay.wndex.lpszClassName, overlay.wndex.hInstance);
}

void reset_device() 
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    if (directx9.device->Reset(&directx9.params) == D3DERR_INVALIDCALL) IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

static ImVec2 ScreenRes, WindowPos;

bool create_text_ctx(PDIRECT3DTEXTURE9* out_texture, LPCVOID data_src, size_t data_size)
{
    PDIRECT3DTEXTURE9 texture;
    HRESULT hr = D3DXCreateTextureFromFileInMemory(window_class::device(), data_src, data_size, &texture);
    if (hr != S_OK) return false;

    D3DSURFACE_DESC my_image_desc;
    texture->GetLevelDesc(0, &my_image_desc);
    *out_texture = texture;

    return true;
}

void wnd_center()
{
    RECT rectClient, rectWindow;
    HWND hWnd = overlay.hwnd;
    GetClientRect(hWnd, &rectClient);
    GetWindowRect(hWnd, &rectWindow);

    int width = rectWindow.right - rectWindow.left;
    int height = rectWindow.bottom - rectWindow.top;

    MoveWindow(
        hWnd,
        GetSystemMetrics(SM_CXSCREEN) / 2 - (width) / 2, GetSystemMetrics(SM_CYSCREEN) / 2 - (height) / 2,
        rectClient.right - rectClient.left, rectClient.bottom - rectClient.top, TRUE
    );
}

HICON GetParentProcessIcon() 
{
    DWORD parentProcessId = {};
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(hSnapshot, &pe32)) 
    {
        do {
            if (pe32.th32ProcessID == GetCurrentProcessId()) 
            {
                CloseHandle(hSnapshot);
                parentProcessId = pe32.th32ParentProcessID;
            }
        } 
        while (Process32Next(hSnapshot, &pe32));
    }

    HANDLE hParentProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, parentProcessId);

    if (hParentProcess == nullptr) 
    {
        return nullptr;
    }

    WCHAR parentProcessExePath[MAX_PATH] = { 0 };
    DWORD bufferSize                     = sizeof(parentProcessExePath) / sizeof(WCHAR);

    if (GetModuleFileName(nullptr, (LPSTR)parentProcessExePath, bufferSize) == 0) 
    {
        CloseHandle(hParentProcess);
        return nullptr;
    }

    HICON hIcon;
    ExtractIconEx((LPSTR)parentProcessExePath, 0, nullptr, &hIcon, 1);

    CloseHandle(hParentProcess);
    return hIcon;
}

bool Application::Create(std::string& m_window_title, void (*Handler)(void))
{
    overlay.name        = { appinfo.Title.c_str() };
    overlay.wndex       = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), 0, NULL, NULL, NULL, overlay.name, 0 };
    overlay.wndex.hIcon = { GetParentProcessIcon()};

    RegisterClassExA(&overlay.wndex);

    overlay.hwnd = CreateWindowA(overlay.name, overlay.name, WS_POPUP | WS_VISIBLE, 0, 0, appinfo.width, appinfo.height, NULL, NULL, GetModuleHandle(nullptr), NULL);

    if (!create_device_d3d(overlay.hwnd)) 
    { 
        clear_all(); 
    }

    //alow window border-radius windows 11 specific.
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    MARGINS margins           = { -1 };

    DwmSetWindowAttribute(overlay.hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
    DwmExtendFrameIntoClientArea(overlay.hwnd, &margins);

    ::wnd_center();::ShowWindow(overlay.hwnd, SW_SHOW);::UpdateWindow(overlay.hwnd);

    SetWindowPos(overlay.hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(overlay.hwnd);

    ImGui::SetCurrentContext(ImGui::CreateContext());

    ImGuiIO* IO = &ImGui::GetIO();
    IO->Fonts->AddFontFromMemoryTTF(Memory::Poppins, sizeof Memory::Poppins, 16);
    IO->Fonts->AddFontFromMemoryTTF(Memory::Poppins, sizeof Memory::Poppins, 13);
    IO->Fonts->AddFontFromMemoryTTF(Memory::Poppins, sizeof Memory::Poppins, 20);

    IO->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
   
    directx9.params.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;
	directx9.params.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

    create_text_ctx(&icons_struct.icon, Memory::logo, sizeof Memory::logo);

    create_text_ctx(&icons_struct.trash_icon, Memory::trash_icon, sizeof Memory::trash_icon);
    create_text_ctx(&icons_struct.skin_icon, Memory::skin_icon, sizeof Memory::skin_icon);
    create_text_ctx(&icons_struct.check_mark_checked, Memory::icon_check_mark_full, sizeof Memory::icon_check_mark_full);
    create_text_ctx(&icons_struct.check_mark_unchecked, Memory::icon_check_mark_empty, sizeof Memory::icon_check_mark_empty);
    create_text_ctx(&icons_struct.reload_icon, Memory::icon_reload, sizeof Memory::icon_reload);
    create_text_ctx(&icons_struct.icon_no_results, Memory::icon_no_results, sizeof Memory::icon_no_results);
    create_text_ctx(&icons_struct.icon_remote_skin, Memory::online_skin_icon, sizeof Memory::online_skin_icon);
    create_text_ctx(&icons_struct.more_icon, Memory::more_icon, sizeof Memory::more_icon);
    create_text_ctx(&icons_struct.icon_saved, Memory::icon_is_saved, sizeof Memory::icon_is_saved);

    create_text_ctx(&icons_struct.close, Memory::close, sizeof Memory::close);
    create_text_ctx(&icons_struct.minimize, Memory::minimize, sizeof Memory::minimize);

    create_text_ctx(&icons_struct.exit, Memory::exit, sizeof Memory::exit);

    if (ImGui_ImplWin32_Init(overlay.hwnd) && ImGui_ImplDX9_Init(directx9.device))
    {
        set_proc_theme_colors();
        memset((&directx9.msg), 0, (sizeof(directx9.msg)));

        while (directx9.msg.message != WM_QUIT)
        {
            if (PeekMessageA(&directx9.msg, NULL, (UINT)0U, (UINT)0U, PM_REMOVE))
            {
                TranslateMessage(&directx9.msg);
                DispatchMessageA(&directx9.msg);
                continue;
            }

            ImGui_ImplDX9_NewFrame(); 
            ImGui_ImplWin32_NewFrame();

            ImGui::NewFrame();
            {
                static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
                ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);

                ImGui::Begin(appinfo.Title.c_str(), nullptr, flags); Handler(); ImGui::End();
            }
            ImGui::EndFrame();

            directx9.device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

            if (directx9.device->BeginScene() >= 0) 
            { 
                ImGui::Render(); 
                ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData()); 
                directx9.device->EndScene(); 
            }

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) 
            {
                ImGui::UpdatePlatformWindows();
            }
            
            ImGui::RenderPlatformWindowsDefault();

            if (directx9.device->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST && directx9.device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) 
            {
                reset_device();
            }
        }

        IO->Fonts->ClearFonts();

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();

        clear_all();
        ::DestroyWindow(overlay.hwnd);

    }
    return false;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    {
        return true;
    }

    static tagPOINT last_loc;

    switch (msg) 
    {
        case WM_LBUTTONDOWN: 
        {
            if (!ImGui::IsAnyItemHovered())
            {
                tagRECT rect;
                mousedown = true;
                GetCursorPos(&last_loc);
                GetWindowRect(overlay.hwnd, &rect);

                last_loc.x = last_loc.x - rect.left;
                last_loc.y = last_loc.y - rect.top;
            }
            break;
        }
        case WM_LBUTTONUP: 
        {
            mousedown = false;
            break;
        }
        case WM_MOUSEMOVE: 
        {
            if (mousedown && !ImGui::IsAnyItemHovered())
            {
                POINT currentpos;
                GetCursorPos(&currentpos);

                uint16_t x = currentpos.x - last_loc.x;
                uint16_t y = currentpos.y - last_loc.y;

                MoveWindow(overlay.hwnd, x, y, appinfo.width, appinfo.height, false);
            }
            break;
        }
        case WM_SIZE:
        {
            if (directx9.device != NULL && wParam != SIZE_MINIMIZED)
            {
                directx9.params.BackBufferWidth = LOWORD(lParam);
                directx9.params.BackBufferHeight = HIWORD(lParam);
                reset_device();
            }
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            ((MINMAXINFO*)lParam)->ptMinTrackSize = { 750, 500 };
            return 0;
        }
        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void set_proc_theme_colors()
{
	ImGuiStyle* sty = (ImGuiStyle*)(void*) & ImGui::GetStyle();

    sty->FrameRounding     = 2.0f;
    sty->ChildRounding     = 3.0f;
    sty->GrabRounding      = 4.0f;
    sty->ItemSpacing       = ImVec2(12, 8);
    sty->ScrollbarSize     = 8.0f;
    sty->ScrollbarRounding = 8.0f;

	sty->SetTheme[ImGuiCol_ChildBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty->SetTheme[ImGuiCol_Border]               = ImVec4(0.18, 0.18, 0.18, 1.00f);
      
	sty->SetTheme[ImGuiCol_Text]                 = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
	sty->SetTheme[ImGuiCol_TextDisabled]         = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	sty->SetTheme[ImGuiCol_WindowBg]             = ImVec4(0.10, 0.10, 0.10, 1.00f);
	sty->SetTheme[ImGuiCol_ScrollbarBg]          = ImVec4(0.12, 0.12, 0.12, 1.00f);
	sty->SetTheme[ImGuiCol_PopupBg]              = ImVec4(0.10, 0.10, 0.10, 1.00f);
	sty->SetTheme[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty->SetTheme[ImGuiCol_FrameBgHovered]       = ImVec4(0.18, 0.18, 0.18, 1.00f);
	sty->SetTheme[ImGuiCol_FrameBgActive]        = ImVec4(0.18, 0.18, 0.18, 1.00f);
	sty->SetTheme[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty->SetTheme[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.14f, 0.14f, 0.14f, 0.75f);
	sty->SetTheme[ImGuiCol_TitleBgActive]        = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty->SetTheme[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);

	sty->SetTheme[ImGuiCol_Button]               = ImVec4(0.14, 0.14, 0.14, 1.00f);
	sty->SetTheme[ImGuiCol_ButtonHovered]        = ImVec4(0.16, 0.16, 0.16, 1.00f);
	sty->SetTheme[ImGuiCol_ButtonActive]         = ImVec4(0.16, 0.16, 0.16, 1.00f);
	sty->SetTheme[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	sty->SetTheme[ImGuiCol_TabHovered]           = ImVec4(0.16, 0.16, 0.16, 1.00f);
	sty->SetTheme[ImGuiCol_TabActive]            = ImVec4(0.16, 0.16, 0.16, 1.00f);
	sty->SetTheme[ImGuiCol_Header]               = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	sty->SetTheme[ImGuiCol_HeaderHovered]        = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	sty->SetTheme[ImGuiCol_HeaderActive]         = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	sty->SetTheme[ImGuiCol_ResizeGrip]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	sty->SetTheme[ImGuiCol_ResizeGripHovered]    = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	sty->SetTheme[ImGuiCol_ResizeGripActive]     = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	sty->SetTheme[ImGuiCol_PlotLines]            = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	sty->SetTheme[ImGuiCol_PlotLinesHovered]     = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	sty->SetTheme[ImGuiCol_PlotHistogram]        = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	sty->SetTheme[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	sty->SetTheme[ImGuiCol_TextSelectedBg]       = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
}
