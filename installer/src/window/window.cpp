#include <Windows.h>
#include <TlHelp32.h>
#include <Shlwapi.h>

#include <src/window/window.hpp>

#include <src/vendor/imgui/imgui.h>
#include <src/vendor/imgui/imgui_internal.h>
#include <vector>

#include <strsafe.h>
#include <src/window/memory.h>

#include <d3d9.h>
#include <d3dx9tex.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

#include <stdexcept>
#include <system_error>

#include <Windows.h>
#include <windowsx.h>
#include <functional>

#include <scripts/resource.h>

static bool mousedown = false;
bool app_open = true;

bool g_headerHovered = false;
bool g_headerHovered_1 = false;

bool g_windowOpen = false;

icons icons_struct = {};
icons Window::iconsObj() { return icons_struct; }

void set_proc_theme_colors();

struct dx9_interface { LPDIRECT3D9 ID3D9; LPDIRECT3DDEVICE9 device; D3DPRESENT_PARAMETERS params; MSG msg; } directx9;
struct overlay_window { WNDCLASSEXW wndex; HWND hwnd; LPCSTR name; } overlay;
struct app_ctx { std::string Title; uint32_t width = 1215, height = 850; } appinfo;

void Window::setTitle(char* name) { appinfo.Title = name; }
void Window::setDimensions(ImVec2 dimensions) { appinfo.width = (int)dimensions.x; appinfo.height = (int)dimensions.y; }

HWND Window::getHWND() { return overlay.hwnd; }

void Window::bringToFront()
{
    DWORD threadId = ::GetCurrentThreadId();
    DWORD processId = ::GetWindowThreadProcessId(overlay.hwnd, NULL);

    ::AttachThreadInput(processId, threadId, TRUE);
    ::SetWindowPos(overlay.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    ::SetWindowPos(overlay.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
    ::SetForegroundWindow(overlay.hwnd);
    ::SetFocus(overlay.hwnd);
    ::SetActiveWindow(overlay.hwnd);
    ::AttachThreadInput(processId, threadId, FALSE);
}

LPDIRECT3DDEVICE9* Window::getDevice() { return &directx9.device; }

bool create_device_d3d(HWND hWnd)
{
    if ((directx9.ID3D9 = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
    {
        return false;
    }
    ZeroMemory(&directx9.params, sizeof(directx9.params));

    directx9.params.Windowed = TRUE;
    directx9.params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    directx9.params.BackBufferFormat = D3DFMT_UNKNOWN;
    directx9.params.EnableAutoDepthStencil = TRUE;
    directx9.params.AutoDepthStencilFormat = D3DFMT_D16;
    directx9.params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (directx9.ID3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &directx9.params, &directx9.device) < 0)
    {
        return false;
    }
    return true;
}

void clear_all()
{
    if (directx9.device) { 
        directx9.device->Release(); 
        directx9.device = nullptr; 
    }
    if (directx9.ID3D9) { 
        directx9.ID3D9->Release();  
        directx9.ID3D9 = nullptr; 
    }

    UnregisterClassW(overlay.wndex.lpszClassName, overlay.wndex.hInstance);
}

void reset_device()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    if (directx9.device->Reset(&directx9.params) == D3DERR_INVALIDCALL) IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

static ImVec2 ScreenRes, WindowPos;

bool createRawImage(PDIRECT3DTEXTURE9* out_texture, LPCVOID data_src, size_t data_size)
{
    PDIRECT3DTEXTURE9 texture;
    HRESULT hr = D3DXCreateTextureFromFileInMemory(*Window::getDevice(), data_src, data_size, &texture);
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
        } while (Process32Next(hSnapshot, &pe32));
    }

    HANDLE hParentProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, parentProcessId);

    if (hParentProcess == nullptr)
    {
        return nullptr;
    }

    WCHAR parentProcessExePath[MAX_PATH] = { 0 };
    DWORD bufferSize = sizeof(parentProcessExePath) / sizeof(WCHAR);

    if (GetModuleFileName(nullptr, (LPSTR)parentProcessExePath, bufferSize) == 0)
    {
        CloseHandle(hParentProcess);
        return nullptr;
    }

    HICON hIcon;
    ExtractIconExA((LPSTR)parentProcessExePath, 0, nullptr, &hIcon, 1);

    CloseHandle(hParentProcess);
    return hIcon;
}

void initFontAtlas(ImGuiIO* IO)
{
    IO->Fonts->AddFontFromMemoryTTF(Memory::Poppins, sizeof Memory::Poppins, 16);
    IO->Fonts->AddFontFromMemoryTTF(Memory::Poppins, sizeof Memory::Poppins, 13);
    IO->Fonts->AddFontFromMemoryTTF(Memory::Poppins, sizeof Memory::Poppins, 20);

    IO->Fonts->AddFontFromMemoryTTF(Memory::cascadia, sizeof Memory::cascadia, 14);
}

void initTextures()
{
    //createRawImage(&icons_struct.planet, Memory::planet, sizeof Memory::planet);

    createRawImage(&icons_struct.maximize, Memory::maximize, sizeof Memory::maximize);
    createRawImage(&icons_struct.minimize, Memory::minimize, sizeof Memory::minimize);
    createRawImage(&icons_struct.close, Memory::close, sizeof Memory::close);

    createRawImage(&icons_struct.greyed_out, Memory::greyed_out, sizeof Memory::greyed_out);
}

bool Application::Destroy()
{
    PostQuitMessage(0);

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    //ImGui::DestroyContext();

    clear_all();
    ::DestroyWindow(overlay.hwnd);
    ::UnregisterClassW(overlay.wndex.lpszClassName, overlay.wndex.hInstance);

    return true;
}

enum class Style : DWORD {
    windowed = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
    aero_borderless = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
    basic_borderless = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
};

auto composition_enabled() -> bool {
    BOOL composition_enabled = FALSE;
    bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
    return composition_enabled && success;
}

auto select_borderless_style() -> Style {
    return composition_enabled() ? Style::aero_borderless : Style::basic_borderless;
}

auto set_shadow(HWND handle, bool enabled) -> void {
    if (composition_enabled()) {
        static const MARGINS shadow_state[2]{ { 0,0,0,0 },{ 1,1,1,1 } };
        ::DwmExtendFrameIntoClientArea(handle, &shadow_state[enabled]);
    }
}

//void set_borderless_shadow(bool enabled) {
//    if (borderless) {
//        borderless_shadow = enabled;
//        set_shadow(handle.get(), enabled);
//    }
//}

void set_borderless(bool enabled) {
    Style new_style = (enabled) ? select_borderless_style() : Style::windowed;
    Style old_style = static_cast<Style>(::GetWindowLongPtrW(Window::getHWND(), GWL_STYLE));

    if (new_style != old_style) {
        ::SetWindowLongPtrW(Window::getHWND(), GWL_STYLE, static_cast<LONG>(new_style));

        // when switching between borderless and windowed, restore appropriate shadow state
        set_shadow(Window::getHWND(), true && (new_style != Style::windowed));

        // redraw frame
        ::SetWindowPos(Window::getHWND(), nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
        ::ShowWindow(Window::getHWND(), SW_SHOW);
    }
}

//auto window_class(WNDPROC wndproc) -> const wchar_t* {
//    static const wchar_t* window_class_name = [&] {
//
//    }();
//    return window_class_name;
//}

bool borderless = true;
bool borderless_shadow = true;
bool borderless_drag = true;
bool borderless_resize = false;

const void Window::center_modal(ImVec2 size) {
    RECT rect;
    GetWindowRect(Window::getHWND(), &rect);

    float xPos = rect.left + (rect.right - rect.left - size.x) / 2;
    float yPos = rect.top + (rect.bottom - rect.top - size.y) / 2;

    ImGui::SetNextWindowPos(ImVec2(xPos, yPos));
    ImGui::SetNextWindowSize(size);
}

void set_borderless_shadow(bool enabled) {
    if (borderless) {
        borderless_shadow = enabled;
        set_shadow(Window::getHWND(), enabled);
    }
}


auto maximized(HWND hwnd) -> bool {
    WINDOWPLACEMENT placement;
    if (!::GetWindowPlacement(hwnd, &placement)) {
        return false;
    }

    return placement.showCmd == SW_MAXIMIZE;
}

auto adjust_maximized_client_rect(HWND window, RECT& rect) -> void {
    if (!maximized(window)) {
        return;
    }

    auto monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    if (!monitor) {
        return;
    }

    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) {
        return;
    }

    // when maximized, make the client area fill just the monitor (without task bar) rect,
    // not the whole window rect which extends beyond the monitor.
    rect = monitor_info.rcWork;
}

ImVec2 g_windowPadding = {};

bool Application::Create(std::function<void()> Handler, HINSTANCE hInstance)
{
    overlay.name = { appinfo.Title.c_str() };
    //RegisterClassExA(&overlay.wndex);

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));


    WNDCLASSEXW wcx{};
    wcx.cbSize = sizeof(wcx);
    wcx.hInstance = hInstance;
    wcx.lpfnWndProc = WndProc;
    wcx.lpszClassName = L"Millennium";
    //wcx.hIcon = { GetParentProcessIcon() };
    wcx.hCursor = ::LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    const ATOM result = ::RegisterClassExW(&wcx);

    overlay.wndex = wcx;

    overlay.hwnd = CreateWindowExW(
        0, overlay.wndex.lpszClassName, (LPCWSTR)overlay.name,
        static_cast<DWORD>(Style::aero_borderless), CW_USEDEFAULT, CW_USEDEFAULT,
        450, 325, nullptr, nullptr, nullptr, nullptr
    );

    SendMessage(overlay.hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(overlay.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);


    ::wnd_center();
    set_borderless(true);
    set_borderless_shadow(true);
    //set_borderless_shadow(true);

    if (!create_device_d3d(overlay.hwnd))
    {
        clear_all();
    }

    //alow window border-radius windows 11 specific.
    //DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    //MARGINS margins           = { -1 };

    //DwmSetWindowAttribute(overlay.hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
    //DwmExtendFrameIntoClientArea(overlay.hwnd, &margins);
    ::ShowWindow(overlay.hwnd, SW_SHOW); ::UpdateWindow(overlay.hwnd);

    SetWindowPos(overlay.hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    Window::bringToFront();

    ImGui::SetCurrentContext(ImGui::CreateContext());

    ImGuiIO* IO = &ImGui::GetIO();
    initFontAtlas(IO);

    IO->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    directx9.params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    directx9.params.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

    g_windowOpen = true;

    initTextures();

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

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, g_windowPadding);

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


auto hit_test(POINT cursor) -> LRESULT {
    // identify borders and corners to allow resizing the window.
    // Note: On Windows 10, windows behave differently and
    // allow resizing outside the visible window frame.
    // This implementation does not replicate that behavior.
    const POINT border{
        ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
        ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
    };
    RECT window;
    if (!::GetWindowRect(Window::getHWND(), &window)) {
        return HTNOWHERE;
    }

    const auto drag = borderless_drag ? HTCAPTION : HTCLIENT;

    enum region_mask {
        client = 0b0000,
        left = 0b0001,
        right = 0b0010,
        top = 0b0100,
        bottom = 0b1000,
    };

    const auto result =
        left * (cursor.x < (window.left + border.x)) |
        right * (cursor.x >= (window.right - border.x)) |
        top * (cursor.y < (window.top + border.y)) |
        bottom * (cursor.y >= (window.bottom - border.y));

    switch (result) {
    case left: return borderless_resize ? HTLEFT : drag;
    case right: return borderless_resize ? HTRIGHT : drag;
    case top: return borderless_resize ? HTTOP : drag;
    case bottom: return borderless_resize ? HTBOTTOM : drag;
    case top | left: return borderless_resize ? HTTOPLEFT : drag;
    case top | right: return borderless_resize ? HTTOPRIGHT : drag;
    case bottom | left: return borderless_resize ? HTBOTTOMLEFT : drag;
    case bottom | right: return borderless_resize ? HTBOTTOMRIGHT : drag;
    case client: {
        if (g_headerHovered || g_headerHovered_1) {
            return drag;
        }
        else return HTCLIENT;
    }
    default: return HTNOWHERE;
    }
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
    case WM_SIZE:
    {
        if (wParam == SIZE_MAXIMIZED) {
            g_windowPadding = ImVec2(10, 10);
        }
        else {
            g_windowPadding = ImVec2(0, 0);
        }

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
    case WM_NCCALCSIZE: {
        return 0;
    }
    case WM_NCHITTEST: {
        // When we have no border or title bar, we need to perform our
        // own hit testing to allow resizing and moving.
        if (borderless) {
            return hit_test(POINT{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
                });
        }
        break;
    }
    case WM_NCACTIVATE: {
        //if (!composition_enabled()) {
        //    // Prevents window frame reappearing on window activation
        //    // in "basic" theme, where no aero shadow is present.
        //    return 1;
        //}
        return 1;
    }

    case WM_CLOSE: {
        ::DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void set_proc_theme_colors()
{
    ImGuiStyle* sty = (ImGuiStyle*)(void*)&ImGui::GetStyle();

    sty->FrameRounding = 2.0f;
    sty->ChildRounding = 3.0f;
    sty->GrabRounding = 4.0f;
    sty->ItemSpacing = ImVec2(12, 8);
    sty->ScrollbarSize = 10.0f;
    sty->ScrollbarRounding = 8.0f;
    sty->AntiAliasedFill = true;

    sty->SetTheme[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    sty->SetTheme[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    sty->SetTheme[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
    sty->SetTheme[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    sty->SetTheme[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    sty->SetTheme[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    sty->SetTheme[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    sty->SetTheme[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    sty->SetTheme[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    sty->SetTheme[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    sty->SetTheme[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    sty->SetTheme[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 0.75f);
    sty->SetTheme[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    sty->SetTheme[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);

    sty->SetTheme[ImGuiCol_Button] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    sty->SetTheme[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    sty->SetTheme[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    sty->SetTheme[ImGuiCol_Tab] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    sty->SetTheme[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    sty->SetTheme[ImGuiCol_TabActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

    sty->SetTheme[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    sty->SetTheme[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    sty->SetTheme[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    sty->SetTheme[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    sty->SetTheme[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    sty->SetTheme[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    sty->SetTheme[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    sty->SetTheme[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    sty->SetTheme[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    sty->SetTheme[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    sty->SetTheme[ImGuiCol_TextSelectedBg] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
}
