#include "stdafx.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <window/core/window.hpp>
#include <GLFW/glfw3native.h>

#ifdef _WIN32
#include <Windows.h> // include windows library on windows
#include <windowsx.h>
#endif

/**
 * The current version of Millennium. Responsible for deciding when to update 
 */
const char* m_ver = "1.1.4";

void OpenURL(std::string url) {
#ifdef _WIN32
	ShellExecute(0, "open", url.c_str(), 0, 0, SW_SHOWNORMAL);
#elif __linux__
	system(fmt::format("xdg-open {} &", url.c_str() ).c_str());
#endif
}


bool g_messageHover = false;
bool g_messageOpen = true;

#ifdef _WIN32

HWND messageHwnd;
WNDPROC messageProc;

auto message_hit_test(POINT cursor) -> LRESULT {
    // identify borders and corners to allow resizing the window.
    // Note: On Windows 10, windows behave differently and
    // allow resizing outside the visible window frame.
    // This implementation does not replicate that behavior.
    const POINT border{
        2,
        2
    };

    RECT window;
    if (!::GetWindowRect(messageHwnd, &window)) {
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
        if (g_messageHover && cursor.x <= window.right - 25) {
            return drag;
        }
        else {
            return HTCLIENT;
        }
    }
    default: return HTNOWHERE;
    }
}

LRESULT CALLBACK MsgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static tagPOINT last_loc;

    switch (msg)
    {
    case WM_NCCALCSIZE: {
        return 0;
    }
    case WM_NCHITTEST: {
        return message_hit_test(POINT{
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
    return CallWindowProc(messageProc, hWnd, msg, wParam, lParam);
}

#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void MsgHandler(std::string header, std::function<void(bool* open)> cb)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(325, 130, "Millennium", nullptr, nullptr);
    if (window == nullptr)
        return;

    glfwMakeContextCurrent(window);

#ifdef _WIN32
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);

    messageHwnd = glfwGetWin32Window(window);
    SetWindowPos(messageHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    bringToFront(messageHwnd);
    wnd_center(messageHwnd);

    messageProc = (WNDPROC)GetWindowLongPtr(messageHwnd, GWLP_WNDPROC);
    SetWindowLongPtr(messageHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MsgWndProc));
    SetWindowPos(messageHwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

    SendMessage(messageHwnd, WM_SYSCOMMAND, SC_RESTORE, 0); // restore the minimize window
    SetForegroundWindow(messageHwnd);
    SetActiveWindow(messageHwnd);
    SetWindowPos(messageHwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    //redraw to prevent the window blank.
    RedrawWindow(messageHwnd, NULL, 0, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

    FLASHWINFO fi;
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = messageHwnd;
    fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
    fi.uCount = 1000;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);

    std::thread([&]() { PlaySoundA("SystemExclamation", NULL, SND_ALIAS); }).detach();
#endif 
    glfwSwapInterval(1);

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

    initFontAtlas();
    set_proc_theme_colors();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            static ImGuiWindowFlags flags = /*ImGuiWindowFlags_NoDecoration |*/ ImGuiWindowFlags_NoCollapse /*| ImGuiWindowFlags_MenuBar*/ /*| ImGuiWindowFlags_NoMove  */ | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

            //ImGui::SetNextWindowSize(ImVec2((float)appinfo.width, (float)appinfo.height));

            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
            ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);

            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));

            ImGui::Begin(header.c_str(), nullptr, flags);
            {
                ImGui::PopStyleColor();

                g_messageHover = ImGui::IsItemHovered();
                cb(&g_messageOpen);

                glfwSetWindowSize(window, 325, ImGui::GetCursorPosY() + 5);
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

        if (!g_messageOpen)
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

    g_messageOpen = false;
    console.log("exiting message box");
}

void MsgBox(std::string header, std::function<void(bool* open)> cb) {
    MsgHandler(header, cb);
    //std::thread([&]() {
    //    MsgHandler(header, cb);
    //}).detach();
}