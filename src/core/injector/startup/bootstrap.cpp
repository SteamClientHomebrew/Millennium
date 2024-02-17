#include <core/injector/startup/bootstrap.hpp>
#include <Windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Popups.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.applicationmodel.h>

#include <utils/thread/thread_handler.hpp>
#include <window/win_handler.hpp>

using namespace winrt;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;
using namespace Windows::UI::Popups;

const bool bootstrapper::parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer, bool _checkForUpdates)
{
	std::filesystem::path skin_json_path = entry.path() / "skin.json";

	if (!std::filesystem::exists(skin_json_path))
		return false;

	auto data = m_Client.readFileSync(skin_json_path.string());

	data["native-name"] = entry.path().filename().string();
	data["name"]        = data.value("name", entry.path().filename().string()).c_str();

	buffer.push_back(data);
	return true;
}

void OnToastActivated(ToastNotification const& sender, winrt::Windows::Foundation::IInspectable const&)
{
	OpenURL(winrt::to_string(sender.Content().GetElementsByTagName(L"updateData").GetAt(0).InnerText()).c_str());
}

const void toastNotification(std::string title, std::string message, std::string url) {

	hstring xml = winrt::to_hstring(std::format(R"(
		<toast>
			<visual>
				<binding template='ToastText02'>
					<text id='1'>{}</text>
					<text id='2'>View Changes</text>
				</binding>
			</visual>
			<updateData>{}</updateData>
		</toast>)",
	message, url));

	winrt::Windows::Data::Xml::Dom::XmlDocument toastXml;
	toastXml.LoadXml(xml);

	ToastNotifier toastNotifier { ToastNotificationManager::CreateToastNotifier(winrt::to_hstring(title)) };
	ToastNotification toastNotification(toastXml);

	toastNotification.Activated({ &OnToastActivated });
	toastNotifier.Show(toastNotification);
}

const void check_updates(nlohmann::basic_json<> theme) {

	std::string nativeName = theme["native-name"];

	if (!theme.contains("update_required") || !theme["update_required"]) {
		return;
	}

	Community::Themes->installUpdate(theme);

	m_Client.parseSkinData(
		/*checkForUpdates*/ true, 
		/*setNewCommit?*/true, 
		/*Specify Theme to reset*/nativeName
	);

	if (Settings::Get<std::string>("active-skin") == nativeName) {

		// The current selected skin was updated so refresh the skin
		steam_js_context SteamJSContext;
		SteamJSContext.reload();
	}

	if (!Settings::Get<bool>("auto-update-themes-notifs")) {
		return;
	}

	std::string themeName = theme["name"].get<std::string>();
	std::string url = theme["git"]["url"].get<std::string>();

	toastNotification("Millennium", std::format("{} was successfully updated!", themeName), url);
}

const void bootstrapper::init(bool setCommit, std::string newCommit) {

	if (Settings::Get<bool>("auto-update-themes") == false) {
		console.log("Auto update themes is disabled, skipping theme updates.");
		return;
	}

	const std::string steamPath = config.getSkinDir();

	std::vector<nlohmann::basic_json<>> jsonBuffer;

	if (!std::filesystem::exists(steamPath)) {
		return;
	}

	for (const auto& entry : std::filesystem::directory_iterator(steamPath)) {
		if (!entry.is_directory() || !this->parseLocalSkin(entry, jsonBuffer, false)) {
			continue;
		}
	}

	try {
		jsonBuffer = m_Client.get_update_list(jsonBuffer, setCommit, newCommit);
	}
	catch (const http_error ex) {
		console.err("HTTP error, can't check for updates on themes");
	}
	catch (const nlohmann::detail::exception& ex) {
		console.err("NLOHMANN Error while checking for updates on themes: " + std::string(ex.what()));
	}
	catch (const std::exception& ex) {
		console.err("STD Error while checking for updates on themes: " + std::string(ex.what()));
	}

	// check for updates on every individual valid theme
	for (const auto& obj : jsonBuffer) {
		check_updates(obj);
	}
}