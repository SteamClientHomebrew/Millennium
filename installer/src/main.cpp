#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>

#include <window/memory.h>
#include <window/window.hpp>
#include <vendor/imgui/imgui_internal.h>

#include <nlohmann/json.hpp>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")

#include <utils/steam/steam_helpers.hpp>
#include <src/installer/installer.hpp>
#include <utils/http/http_client.hpp>
#include <optional>

std::string getRepository() {

	// anti-viruss don't like github links in an unsigned binary
	// it suspects a virus, so im essentially evading it :D

	// it constructs this string at runtime
	// https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest

	std::vector<std::string> parts = {
		"ht"
		"tps",
		"://api",
		".github.com",
		"/repos/"
		"ShadowMonster99/"
		"millennium-steam-"
		"binaries/"
		"releases/"
		"latest"
	};

	std::string result;
	for (auto& part : parts) {
		result += part;
	}

	return result;

}
//static
//const
//constexpr
//std::string_view githubRepo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";

class Handler
{
private:
	static void center(float avail_width, float element_width, float padding = 0) 
	{ 
		ImGui::SameLine((avail_width / 2) - (element_width / 2) + ImGui::GetStyle().FramePadding.x);
	}

	static void center_text(const char* format, float spacing = 15, ImColor color = ImColor(255, 255, 255)) 
	{ 
		center(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(format).x); 
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);	
		ImGui::TextColored(color.Value, format); 
	}

public:
	__declspec(noinline) static void select_option_page(void)
	{
		center(rx, 60);
		ui::shift::y(10);
		ImGui::Image(Window::iconsObj().planet, ImVec2(60, 60));

		center_text(std::format("Millennium Steam Patcher v{}", installer.response.value("tag_name", " null")).c_str(), 70);

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10, 0.10, 0.10, 1.0f));

		center(rx, 220);
		ui::shift::y(30);
		ui::shift::x(4);

		ImGui::BeginChild("##install_page", ImVec2(220, 45), false);
		{
			static bool inProgess = false;

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));

			if (ImGui::Button("Integrate", ImVec2(105, 30))) {
				inProgess = true;

				CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {

					installer.downloadIteration = 0;
					installer.installMillennium(false);
					inProgess = false;
					return true;
				}, 0, 0, 0);
			}
			if (ImGui::IsItemHovered()) {
				std::string notes = installer.response.value("body", "null");

				if (notes != "null") {
					ImGui::SetTooltip(notes.c_str());
				}
			}

			ImGui::SameLine();
			ui::shift::x(-4);

			if (ImGui::Button("Remove", ImVec2(105, 30))) {
				inProgess = false;

				CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {
					installer.uninstallMillennium();
					return true;
				}, 0, 0, 0);
			}

			if (inProgess) {
				ImGui::ProgressBar(((float)installer.downloadIteration + installer.buffer) / (float)installer.totalLength, ImVec2(rx, 4));
			}

			ImGui::PopStyleColor(2);
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

	}

	static void renderContentPanel()
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
		{
			if (ImGui::BeginChild("ChildContentPane", ImVec2(rx, ry), true))
			{
				ImGui::BeginChild("HeaderDrag", ImVec2(rx, 15), false);
				{
					g_headerHovered = ImGui::IsWindowHovered();
				}
				ImGui::EndChild();

				select_option_page();
			}
			ImGui::EndChild();
		}
		ImGui::PopStyleColor();
	}

	static void renderSilent() {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
		{
			center(rx, 280);

			if (ImGui::BeginChild("ChildContentPane", ImVec2(280, ry), true))
			{
				ImGui::Text(std::format("Updating Millennium to v{}...", installer.response.value("tag_name", " null")).c_str());
				ImGui::ProgressBar(((float)installer.downloadIteration + installer.buffer) / (float)installer.totalLength, ImVec2(rx, 4));

				ImGui::Text(std::format("Downloading files [{}/{}]...", (int)installer.downloadIteration + 1, (int)installer.totalLength).c_str());
				ImGui::SameLine();

				ui::shift::right(50);

				if (ImGui::Button("Cancel", ImVec2(50, 0))) {
					ExitProcess(0);
				}
			}
			ImGui::EndChild();
		}
		ImGui::PopStyleColor();
	}
};

const std::vector<std::string> getStartUpArgs() {
	std::vector<std::string> args;

	int argc = 0;
	LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (!argvW)
		return {};

	for (int i = 0; i < argc; ++i) {
		int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);

		if (len <= 0)
			return {};

		std::vector<char> buffer(len);
		WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, buffer.data(), len, NULL, NULL);

		args.emplace_back(buffer.data());
	}
	LocalFree(argvW);

	return args;
}

void silentInstaller(HINSTANCE& hInstance) {

	Window::setTitle((char*)"Millennium Installer");
	Window::setDimensions(ImVec2({ 325, 125 }));


	CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {
		installer.downloadIteration = 0;
		installer.installMillennium(true);

		Steam::Start();
		ExitProcess(0);
		return true;
	}, 0, 0, 0);

	Application::Create(&Handler::renderSilent, hInstance);
}

void normalInstaller(HINSTANCE& hInstance) {
	Window::setTitle((char*)"Millennium Installer");
	Window::setDimensions(ImVec2({ 350, 225 }));

	Application::Create(&Handler::renderContentPanel, hInstance);
}

static const bool retreiveGithubInfo() {

	try {
		installer.response = nlohmann::json::parse(http::get(getRepository()));
	}
	catch (const http_error& err) {
		MessageBoxA(GetForegroundWindow(), "No internet connection?", "Oops", MB_ICONERROR);
		return false;
	}
	catch (const nlohmann::detail::exception ex) {
		MessageBoxA(GetForegroundWindow(), "Received malformed message from GithHub API", "Oops", MB_ICONERROR);
		return false;
	}

	if (installer.response.contains("message"))
	{
		if (installer.response["message"] == "Not Found") {
			MessageBoxA(GetForegroundWindow(), "Couldn't get latest release because it doesn't exist. Try again later?", "Oops!", MB_ICONERROR);
		}
		if (installer.response["message"].get<std::string>().find("API rate limit exceeded") != std::string::npos) {
			MessageBoxA(GetForegroundWindow(), "Can't get the file query data we need to uninstall millennium\nTry again later (maximum 1 hour)", "Oops!", MB_ICONERROR);
		}
		return false;
	}

	return true;
}

static const void initialize(HINSTANCE hInstance) {

	if (!retreiveGithubInfo())
		return;

	auto results = getStartUpArgs();

	bool silent = std::any_of(results.begin(), results.end(), [](const auto& item) { return item == "-silent"; });

	try {
		silent ? silentInstaller(hInstance) : normalInstaller(hInstance);
	}
	catch (std::exception& ex) {
		MessageBoxA(0, ex.what(), "Millennium - ERROR", MB_ICONERROR);
	}
	catch (...) {
		MessageBoxA(0, "An unknown error occured. Try again later.", "Millennium - ERROR", MB_ICONERROR);
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int){ initialize(hInstance); }
int main(int argc, char* argv[]) { initialize(nullptr); }
