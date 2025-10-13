// #include <ftxui/dom/table.hpp>
// #include <ftxui/component/component.hpp>
// #include <ftxui/component/screen_interactive.hpp>
// #include <ftxui/dom/elements.hpp>
// #include <ftxui/component/loop.hpp>

#include "millennium/stdout_tee.h"
#include "millennium/plugin_logger.h"
#include "millennium/backend_mgr.h"
// #include "millennium/ansi_parse.h"
#include "millennium/init.h"
#include "millennium/millennium_api.h"

// #include "millennium/ftxui_components/scroller.h"

#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"

#include <string_view>
#include <stdio.h>
#include <array>
#include <algorithm>
#include <csignal>

// using namespace ftxui;

// inline ftxui::Component ScrollableComponent(std::function<std::string()> get_output)
// {
//     auto last_size = std::make_shared<size_t>(0);

//     auto content = ftxui::Renderer([get_output]
//     {
//         const std::string output = get_output ? get_output() : "";
//         ftxui::Elements lines;
//         std::stringstream ss(output);
//         std::string line;

//         ftxui::FlexboxConfig config;
//         config.wrap = ftxui::FlexboxConfig::Wrap::NoWrap;

//         while (std::getline(ss, line))
//             lines.push_back(flexbox(parseAnsi(line), config));

//         lines.push_back(flexbox({ ftxui::text("") }, config));
//         return ftxui::vbox(lines) | ftxui::flex_shrink;
//     });

//     return ftxui::Scroller(content, last_size, get_output);
// }

// class MemoryGraph
// {
//   public:
//     std::vector<int> operator()(int width, int height) const
//     {
//         std::vector<int> output(width);
//         int dataSize = static_cast<int>(memorySnapshots.size());

//         for (int i = 0; i < width; ++i) {
//             int index = dataSize - width + i;
//             if (index >= 0 && index < dataSize) {
//                 float normalized = static_cast<float>(memorySnapshots[index]) / maxMemory;
//                 output[i] = static_cast<int>(normalized * (height - 1));
//             } else {
//                 output[i] = 0;
//             }
//         }
//         return output;
//     }

//     void addSnapshot(size_t memUsage)
//     {
//         memorySnapshots.push_back(memUsage);
//         if (memUsage > maxMemory)
//             maxMemory = memUsage;
//         if (memorySnapshots.size() > 1000)
//             memorySnapshots.erase(memorySnapshots.begin());
//     }

//     std::string formatBytes(size_t bytes) const
//     {
//         const char* units[] = { "B", "KB", "MB", "GB" };
//         int unit = 0;
//         double size = static_cast<double>(bytes);

//         while (size >= 1024.0 && unit < 3) {
//             size /= 1024.0;
//             unit++;
//         }

//         char buf[32];
//         snprintf(buf, sizeof(buf), "%.2f %s", size, units[unit]);
//         return buf;
//     }

//     std::vector<size_t> memorySnapshots;
//     size_t maxMemory = 1;
// };

// // Add this somewhere in your class that manages plugins
// class PluginMemoryTracker
// {
//   public:
//     std::unordered_map<std::string, std::shared_ptr<MemoryGraph>> pluginGraphs;
//     int frameCounter = 0;

//     void update()
//     {
//         if (++frameCounter >= 30) {
//             frameCounter = 0;
//             auto& backendManager = BackendManager::GetInstance();

//             for (auto& [pluginName, graph] : pluginGraphs) {
//                 size_t mem = backendManager.Lua_GetPluginMemorySnapshotByName(pluginName);
//                 graph->addSnapshot(mem);
//             }
//         }
//     }

//     std::shared_ptr<MemoryGraph> getOrCreateGraph(const std::string& pluginName)
//     {
//         if (pluginGraphs.find(pluginName) == pluginGraphs.end()) {
//             pluginGraphs[pluginName] = std::make_shared<MemoryGraph>();
//         }
//         return pluginGraphs[pluginName];
//     }
// };

// ftxui::Component RenderPluginDetails(const SettingsStore::PluginTypeSchema& plugin, PluginMemoryTracker& tracker)
// {
//     auto& backendManager = BackendManager::GetInstance();
//     bool isRunning = backendManager.IsLuaBackendRunning(plugin.pluginName) || backendManager.IsPythonBackendRunning(plugin.pluginName);

//     auto memory_graph = tracker.getOrCreateGraph(plugin.pluginName);

//     auto tab_index = std::make_shared<int>(0);
//     auto tab_entries = std::make_shared<std::vector<std::string>>(std::initializer_list<std::string>{ "Manage", "Output", "Memory Usage" });
//     auto tab_selection = Menu(tab_entries.get(), tab_index.get(), MenuOption::HorizontalAnimated());

//     // Static storage for all plugins
//     static std::map<std::string, std::shared_ptr<std::string>> pluginInputs;
//     static std::map<std::string, std::vector<std::string>> pluginHistory;

//     // Initialize for this plugin if needed
//     if (pluginInputs.find(plugin.pluginName) == pluginInputs.end()) {
//         pluginInputs[plugin.pluginName] = std::make_shared<std::string>();
//     }

//     auto inputStr = pluginInputs[plugin.pluginName];
//     std::string pluginName = plugin.pluginName;

//     // Create log display component
//     auto logDisplay = ScrollableComponent([pluginName]()
//     {
//         std::string logContent;
//         for (BackendLogger* logger : g_loggerList) {
//             if (logger->GetPluginName(false) == pluginName) {
//                 auto logs = logger->CollectLogs();
//                 for (const auto& entry : logs) {
//                     logContent += entry.message;
//                 }
//                 break;
//             }
//         }

//         if (logContent.empty()) {
//             logContent = "No logs available for this plugin.";
//         }

//         return logContent;
//     });

//     auto detailsTab = [plugin, isRunning]() -> Component
//     {
//         static std::map<std::string, std::shared_ptr<bool>> pluginEnabledStates;
//         static std::map<std::string, bool> pluginPreviousStates;

//         if (pluginEnabledStates.find(plugin.pluginName) == pluginEnabledStates.end()) {
//             pluginEnabledStates[plugin.pluginName] = std::make_shared<bool>(isRunning);
//             pluginPreviousStates[plugin.pluginName] = isRunning;
//         }
//         auto isEnabled = pluginEnabledStates[plugin.pluginName];
//         bool* isEnabled_ptr = isEnabled.get();

//         auto checkbox = Checkbox("Enabled", isEnabled_ptr);
//         checkbox = Renderer(checkbox, [checkbox, isEnabled_ptr, pluginName = plugin.pluginName]()
//         {
//             bool& prevState = pluginPreviousStates[pluginName];
//             if (*isEnabled_ptr != prevState) {
//                 prevState = *isEnabled_ptr;

//                 auto& backendManager = BackendManager::GetInstance();

//                 Millennium_TogglePluginStatus(std::vector<PluginStatus>{
//                     { .pluginName = pluginName, .enabled = *isEnabled_ptr }
//                 });
//             }
//             return checkbox->Render();
//         });

//         return Container::Vertical({ checkbox, Renderer([plugin, isRunning]() -> Element
//         {
//             return vbox({
//                 hbox({ text("Name: "), text(plugin.pluginName) | bold }),
//                 hbox({ text("Status: "), text(isRunning ? "Running" : "Stopped") | color(isRunning ? Color::Green : Color::Red) }),
//                 hbox({ text("Type: "), text(plugin.backendType == SettingsStore::PluginBackendType::Lua ? "Lua" : "Python") }),
//             });
//         }) });
//     }();

//     auto memoryTab = Renderer([memory_graph, &backendManager, pluginName]
//     {
//         // if (BackendManager::GetInstance().HasPythonBackend(pluginName)) {
//         //     return text("Memory tracking not available for Python plugins.") | bold | color(Color::Yellow);
//         // }

//         size_t currentMem = memory_graph->memorySnapshots.empty() ? 0 : memory_graph->memorySnapshots.back();

//         return window(text("Memory Usage"), vbox({
//                                                 graph(std::ref(*memory_graph)) | size(HEIGHT, GREATER_THAN, 10) | color(Color::BlueLight),
//                                                 hbox({
//                                                     text("Current: " + memory_graph->formatBytes(currentMem)),
//                                                     text(" | Peak: " + memory_graph->formatBytes(memory_graph->maxMemory)),
//                                                 }),
//                                             }));
//     });

//     std::vector<Component> tab_items = { detailsTab, logDisplay, memoryTab };
//     auto tab_content = Container::Tab(tab_items, tab_index.get());

//     auto main_container = Container::Vertical({
//         tab_selection,
//         tab_content,
//     });

//     return Renderer(main_container, [tab_selection, tab_content, tab_entries, tab_index]
//     {
//         return vbox({
//             tab_selection->Render(),
//             tab_content->Render() | flex,
//         });
//     });
// }

// ftxui::Component CreatePluginManagerTab()
// {
//     auto& backendManager = BackendManager::GetInstance();
//     auto settingsStore = std::make_shared<SettingsStore>();
//     auto tracker = std::make_shared<PluginMemoryTracker>();
//     auto selected = std::make_shared<int>(0);
//     auto menu_entries = std::make_shared<std::vector<std::string>>();
//     auto previousSelected = std::make_shared<int>(-1);
//     auto menu = Menu(menu_entries.get(), selected.get());
//     auto pluginDetailsComponent = Container::Vertical({});
//     auto container = Container::Horizontal({ menu, pluginDetailsComponent });

//     return Renderer(container, [&backendManager, settingsStore, tracker, selected, previousSelected, menu_entries, menu, pluginDetailsComponent]
//     {
//         // Update memory tracking for all plugins
//         tracker->update();

//         auto allPlugins = settingsStore->ParseAllPlugins();
//         int longest_name_length = 0;
//         menu_entries->clear();
//         menu_entries->reserve(allPlugins.size());

//         for (const auto& plugin : allPlugins) {
//             bool isRunning = backendManager.IsLuaBackendRunning(plugin.pluginName) || backendManager.IsPythonBackendRunning(plugin.pluginName);

//             menu_entries->push_back(plugin.pluginName);
//             if (plugin.pluginName.length() > longest_name_length) {
//                 longest_name_length = plugin.pluginName.length();
//             }

//             // Ensure graph exists for each plugin
//             tracker->getOrCreateGraph(plugin.pluginName);
//         }

//         // Only recreate component when selection changes
//         if (*selected != *previousSelected && *selected >= 0 && *selected < allPlugins.size()) {
//             pluginDetailsComponent->DetachAllChildren();
//             pluginDetailsComponent->Add(RenderPluginDetails(allPlugins[*selected], *tracker));
//             *previousSelected = *selected;
//         }

//         if (*selected >= 0 && *selected < allPlugins.size()) {
//             return hbox({
//                 vbox({
//                     text(""),
//                     menu->Render() | flex,
//                 }) | size(WIDTH, EQUAL, longest_name_length + 6),
//                 separator() | dim,
//                 pluginDetailsComponent->Render() | flex,
//             });
//         }
//         return vbox({
//             text("Configure loaded plugins") | bold,
//             text(""),
//             menu->Render() | flex,
//         });
//     });
// }

// ftxui::Element CreateEnvironmentVariablesTab()
// {
//     std::vector<std::vector<std::string>> rows;
//     rows.push_back({ "Name", "Value" });

//     for (const auto& kv : envVariables) {
//         rows.push_back({ kv.first, kv.second });
//     }

//     Table table(rows);
//     table.SelectAll().Border(ftxui::LIGHT);
//     table.SelectColumn(0).Border(ftxui::LIGHT);
//     table.SelectRow(0).Decorate(ftxui::bold);
//     table.SelectRow(0).SeparatorVertical(ftxui::LIGHT);
//     table.SelectRow(0).Border(ftxui::LIGHT);
//     table.SelectColumns(0, -1).DecorateCells(ftxui::xflex);

//     return table.Render();
// }

int ShowDeveloperTools()
{
    // int tab_index = 0;
    // auto screen = ScreenInteractive::Fullscreen();
    // std::vector<std::string> tab_entries = { "Console", "Plugin Manager", "Environment Variables" };

    // auto tab_selection = Menu(&tab_entries, &tab_index, MenuOption::HorizontalAnimated());

    // std::vector<Component> tab_items = {
    //     ScrollableComponent([&] { return millennium::cout.str(); }),
    //     CreatePluginManagerTab(),
    //     Renderer(CreateEnvironmentVariablesTab),
    // };

    // auto tab_content = Container::Tab(tab_items, &tab_index);
    // auto main_container = Container::Vertical({
    //     tab_selection,
    //     tab_content,
    // });

    // auto main_renderer = Renderer(main_container, [&]
    // {
    //     return vbox({
    //         hbox({
    //             tab_selection->Render() | flex,
    //         }),
    //         tab_content->Render() | flex,
    //     });
    // });

    // Loop loop(&screen, main_renderer);

    // while (!loop.HasQuitted()) {
    //     screen.Post(Event::Custom);
    //     std::string text = "💫 Millennium@" MILLENNIUM_VERSION;

    //     for (size_t i = 0; i < text.size(); ++i) {
    //         screen.PixelAt(screen.dimx() - text.size() + i, 0).character = text[i];
    //     }

    //     loop.RunOnce();
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // }
    // return 0;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Primary background
    colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);  // #131318
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f); // #131318

    colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);

    // Headers
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.38f, 0.50f, 1.00f);

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.38f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

    // Highlights
    colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.70f, 1.00f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.80f, 1.00f, 0.75f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.90f, 1.00f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);

    // Style tweaks
    // style.WindowRounding = 5.0f;
    // style.FrameRounding = 5.0f;
    // style.GrabRounding = 5.0f;
    // style.TabRounding = 5.0f;
    // style.PopupRounding = 5.0f;
    // style.ScrollbarRounding = 5.0f;
    // style.WindowPadding = ImVec2(10, 10);
    // style.FramePadding = ImVec2(6, 4);
    // style.ItemSpacing = ImVec2(8, 6);
    // style.PopupBorderSize = 0.f;

    bool demo = true;
    int nframes = 0;
    float fval = 1.23f;

    while (true) {
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();

        ImGui::NewFrame();

        auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Hello, world!", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("NFrames = %d", nframes++);
        ImGui::Text("Mouse Pos : x = %g, y = %g", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
        ImGui::Text("Time per frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Float:");

        if (ImGui::Button("Helo")) {
        }

        ImGui::SameLine();
        ImGui::SliderFloat("##float", &fval, 0.0f, 10.0f);

        ImGui::End();
        ImGui::Render();

        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();
    return 0;
}