#include <stdafx.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <window/core/window.hpp>
#include <utils/config/config.hpp>
#include <window/interface/globals.h>
#include <window/api/installer.hpp>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "legacy_stdio_definitions")

//#define GLFW_EXPOSE_NATIVE_X11
//#define GLFW_EXPOSE_NATIVE_GLX
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 // expose window handles [glfwGetWin32Window] 
#include <Windows.h> // include windows library on windows
#endif

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "aptos.cpp"

bool g_headerHovered = false; 
bool g_windowOpen = false;

void set_proc_theme_colors();

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
#endif

void initFontAtlas()
{
    ImGuiIO IO = ImGui::GetIO();
    IO.Fonts->AddFontFromMemoryCompressedTTF(aptos_compressed_data, aptos_compressed_size, 17.0f);
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

static void glfw_error_callback(int error, const char* description)
{
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
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(appinfo.width, appinfo.height, "Millennium HostWND", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, drop_callback);

    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    wnd_center(hwnd);

    //set_borderless(false, hwnd);
    ////set_borderless_shadow(true, hwnd);
    //set_shadow((HWND)hwnd, true);
#endif 
    glfwSwapInterval(1); // Enable vsync

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

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool set_top_most = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static ImGuiWindowFlags flags = /*ImGuiWindowFlags_NoDecoration |*/ ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar /*| ImGuiWindowFlags_NoMove  */ | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

            ImGui::SetNextWindowSize(ImVec2((float)appinfo.width, (float)appinfo.height));
            //ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, g_windowPadding);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));

            ImGui::Begin(appinfo.Title.c_str(), &g_windowOpen, flags);
            {
#ifdef _WIN32
                if (!set_top_most) {
                    HWND hwnd = FindWindowA(appinfo.Title.c_str(), appinfo.Title.c_str());

                    std::cout << hwnd << std::endl;

                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    bringToFront(hwnd);

                    set_top_most = true;
                }
#endif

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
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
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
