#include <stdafx.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <window/core/window.hpp>
#include <utils/config/config.hpp>
#include <window/interface/globals.h>
#include <window/api/installer.hpp>
#include "memory.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "legacy_stdio_definitions")

//#define GLFW_EXPOSE_NATIVE_X11
//#define GLFW_EXPOSE_NATIVE_GLX
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 // expose window handles [glfwGetWin32Window] 
#include <Windows.h> // include windows library on windows
#include <windowsx.h>
#endif

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "aptos.cpp"

bool g_headerHovered = false; 
bool g_windowOpen = false;

struct app_ctx { 
    std::string Title; 
    uint32_t width = 1215, height = 850; 
} appinfo;

void Window::setTitle(char* name) { 
    appinfo.Title = name; 
}

void Window::setDimensions(ImVec2 dimensions) { 
    appinfo.width = (int)dimensions.x; 
    appinfo.height = (int)dimensions.y; 
}
//
//HWND Window::getHWND()    { return overlay.hwnd; }
//

#ifdef _WIN32

HWND _hWnd;
WNDPROC original_proc;

void bringToFront(HWND hwnd)
{
    DWORD threadId  = ::GetCurrentThreadId();
    DWORD processId = ::GetWindowThreadProcessId(hwnd, NULL);

    ::AttachThreadInput(processId, threadId, TRUE);
    ::SetWindowPos(hwnd, HWND_TOPMOST  , 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);
    ::SetActiveWindow(hwnd);
    ::AttachThreadInput(processId, threadId, FALSE);
}

void wnd_center(void* hwnd)
{
    RECT rectClient, rectWindow;
    HWND hWnd = (HWND)hwnd;
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

auto hit_test(POINT cursor) -> LRESULT {
    // identify borders and corners to allow resizing the window.
    // Note: On Windows 10, windows behave differently and
    // allow resizing outside the visible window frame.
    // This implementation does not replicate that behavior.
    const POINT border{
        2,
        2
    };

    RECT window;
    if (!::GetWindowRect(_hWnd, &window)) {
        return HTNOWHERE;
    }

    const auto drag = HTCAPTION;

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
        case client: {
            if (g_headerHovered && cursor.x <= window.right - 25) {
                return drag;
            }
            else {
                return HTCLIENT;
            }
        }
        default: return HTNOWHERE;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static tagPOINT last_loc;

    switch (msg)
    {
        case WM_NCCALCSIZE: {
            return 0;
        }
        case WM_NCHITTEST: {
            return hit_test(POINT{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            });
            break;
        }
        case WM_NCACTIVATE: {
            return 1;
        }
        case WM_CLOSE: {
            g_windowOpen = false;
            ::DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY: {
            g_windowOpen = false;
            PostQuitMessage(0);
            return 0;
        }
    }
    return CallWindowProc(original_proc, hWnd, msg, wParam, lParam);
}

#endif

void initFontAtlas()
{
    ImGuiIO IO = ImGui::GetIO();
    IO.Fonts->AddFontFromMemoryCompressedTTF(aptos_compressed_data, aptos_compressed_size, 17.0f);
    //IO.Fonts->AddFontFromMemoryCompressedTTF(Memory::cascadia, sizeof Memory::cascadia, 17.0f);
}

//void initTextures()
//{
//    create_texture(&icons_struct.skin_icon, Memory::themeIcon, sizeof Memory::themeIcon);
//    create_texture(&icons_struct.reload_icon, Memory::icon_reload, sizeof Memory::icon_reload);
//    create_texture(&icons_struct.icon_no_results, Memory::icon_no_results, sizeof Memory::icon_no_results);
//    create_texture(&icons_struct.foldericon, Memory::foldericon, sizeof Memory::foldericon);
//    create_texture(&icons_struct.editIcon, Memory::editIcon, sizeof Memory::editIcon);
//    create_texture(&icons_struct.deleteIcon, Memory::deleteIcon, sizeof Memory::deleteIcon);
//    create_texture(&icons_struct.xbtn, Memory::xbtn, sizeof Memory::xbtn);
//}

bool initCreated = false;
ImVec2 g_windowPadding = {};
bool g_fileDropQueried = false;
bool g_processingFileDrop = false;
bool g_openSuccessPopup = false;
std::string g_fileDropStatus = "";

#define INVALID_WND_ATTR 65539

static void glfw_error_callback(int error, const char* description)
{
    // no idea what causes this error
    if (error == INVALID_WND_ATTR) {
        return;
    }

    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void drop_callback(GLFWwindow* window, int count, const char** paths) {
    std::cout << "Dropped " << count << " files:" << std::endl;
    for (int i = 0; i < count; i++) {
        std::cout << paths[i] << std::endl;
    }
}

bool Application::Create(std::function<void()> Handler, std::function<void()> callBack)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#ifdef _WIN32
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(appinfo.width, appinfo.height, "Millennium", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, drop_callback);

#ifdef _WIN32
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);

    _hWnd = glfwGetWin32Window(window);
    bringToFront(_hWnd);
    wnd_center(_hWnd);

    original_proc = (WNDPROC)GetWindowLongPtr(_hWnd, GWLP_WNDPROC);
    SetWindowLongPtr(_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
    SetWindowPos(_hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

    SendMessage(_hWnd, WM_SYSCOMMAND, SC_RESTORE, 0); // restore the minimize window
    SetForegroundWindow(_hWnd);
    SetActiveWindow(_hWnd);
    SetWindowPos(_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    //redraw to prevent the window blank.
    RedrawWindow(_hWnd, NULL, 0, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

    FLASHWINFO fi;
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = _hWnd;
    fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
    fi.uCount = 0;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
#endif 
    glfwSwapInterval(1); // Enable vsync

    //glfwSetWindowPosCallback(window, windowDragCallback);

    //// Set mouse button callback
    //glfwSetMouseButtonCallback(window, mouseButtonCallback);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows


    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    callBack(); //initTextures();
    initFontAtlas();
    set_proc_theme_colors();

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            static ImGuiWindowFlags flags = /*ImGuiWindowFlags_NoDecoration |*/ ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar /*| ImGuiWindowFlags_NoMove  */ | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

            //ImGui::SetNextWindowSize(ImVec2((float)appinfo.width, (float)appinfo.height));

            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
            ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, g_windowPadding);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));

            ImGui::Begin(appinfo.Title.c_str(), &g_windowOpen, flags);
            {
                ImGui::PopStyleColor();
                g_headerHovered = ImGui::IsItemHovered();
                Handler();
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);

        if (!g_windowOpen)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    g_windowOpen = false;
    return false;
}

void set_proc_theme_colors()
{
	ImGuiStyle& sty = ImGui::GetStyle();

    sty.FrameRounding     = 4.0f;
    sty.ChildRounding     = 0.0f;
    sty.GrabRounding      = 4.0f;
    sty.WindowRounding    = 0.0f;
    //sty.WindowBorderSize = 5.0f;
    //sty.ItemSpacing       = ImVec2(12, 8);
    sty.ScrollbarSize     = 10.0f;
    sty.ScrollbarRounding = 4.0f;
    sty.AntiAliasedFill = true;

	sty.Colors[ImGuiCol_ChildBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty.Colors[ImGuiCol_Border]               = ImVec4(0.25f, 0.25f, 0.25f, 0.0f);
      
	sty.Colors[ImGuiCol_Text]                 = ImVec4(1.f, 1.f, 1.f, 1.00f);
	sty.Colors[ImGuiCol_TextDisabled]         = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
	sty.Colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	sty.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	sty.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	sty.Colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	sty.Colors[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	sty.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	sty.Colors[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.14f, 0.14f, 0.14f, 0.75f);
	sty.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

	sty.Colors[ImGuiCol_Button]               = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	sty.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	sty.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	sty.Colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	sty.Colors[ImGuiCol_TabHovered]           = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	sty.Colors[ImGuiCol_TabActive]            = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

	sty.Colors[ImGuiCol_Header]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	sty.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	sty.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

	sty.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	sty.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	sty.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	sty.Colors[ImGuiCol_PlotLines]            = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	sty.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	sty.Colors[ImGuiCol_PlotHistogram]        = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	sty.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	sty.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
}
