#include <core/injector/startup/bootstrap.hpp>

#include <utils/thread/thread_handler.hpp>
#include <window/win_handler.hpp>

#ifdef _WIN32
#include <Windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Popups.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.applicationmodel.h>


using namespace winrt;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;
using namespace Windows::UI::Popups;
#endif

/**
 * Parses a local skin directory for skin configuration data.
 *
 * This function reads the skin configuration file (skin.json) located in the provided directory entry,
 * populates the buffer with the parsed JSON data, and updates the JSON data with additional information
 * such as native name and name derived from the directory name.
 *
 * @param entry The directory entry representing the local skin directory.
 * @param buffer A vector to store the parsed JSON data for each skin.
 * @param _checkForUpdates A boolean flag indicating whether to check for updates.
 * @return True if the skin configuration file was successfully parsed and added to the buffer, false otherwise.
 */
const bool bootstrapper::parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer, bool _checkForUpdates)
{
	// construct the path to the skin configuration file (skin.json) within the provided directory entry.
	std::filesystem::path skin_json_path = entry.path() / "skin.json";

	// check if the skin.json file is reflected on disk
	if (!std::filesystem::exists(skin_json_path))
		return false;

	// read the contents of the file. 
	auto data = m_Client.readFileSync(skin_json_path.string());

	// add hints inside the JSON structure so we can use it in the notification
	data["native-name"] = entry.path().filename().string();
	data["name"]        = data.value("name", entry.path().filename().string()).c_str();

	// pass out buffer
	buffer.push_back(data);
	return true;
}

#ifdef _WIN32
/**
 * Handles the activation event of a toast notification.
 *
 * This function is called when a toast notification is activated. It extracts the update URL
 * from the content of the toast notification and opens the URL.
 *
 * @param sender The ToastNotification object that triggered the activation event.
 * @param args An IInspectable interface representing additional arguments passed with the event.
 */
void OnToastActivated(ToastNotification const& sender, winrt::Windows::Foundation::IInspectable const&)
{
	OpenURL(winrt::to_string(sender.Content().GetElementsByTagName(L"updateData").GetAt(0).InnerText()).c_str());
}

/**
 * Displays a toast notification with the specified title, message, and associated URL.
 *
 * This function generates a toast notification with the provided title and message,
 * including a button to view changes associated with the provided URL. When the toast
 * notification is activated, the specified URL is opened.
 *
 * @param title The title of the toast notification.
 * @param message The message content of the toast notification.
 * @param url The URL associated with the toast notification.
 */
const void toastNotification(std::string title, std::string message, std::string url) {

	// Construct the XML content for the toast notification, including the message and URL.
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

	// load the XML content into an XmlDocument to refrence later in the toast handler.
	winrt::Windows::Data::Xml::Dom::XmlDocument toastXml;
	toastXml.LoadXml(xml);

	// create a ToastNotifier for displaying the toast notification. afaik this might be deprecated
	// in windows.h because it hasn't received features its C# counterpart has access to.
	ToastNotifier toastNotifier { 
		ToastNotificationManager::CreateToastNotifier(winrt::to_hstring(title)) 
	};

	// create a ToastNotification object with the loaded XML content.
	ToastNotification toastNotification(toastXml);

	// register a callback function to handle the activation event of the toast notification.
	toastNotification.Activated({ &OnToastActivated });
	toastNotifier.Show(toastNotification);
}
#endif

/**
 * Checks for updates for a given theme and performs necessary actions.
 *
 * This function checks whether an update is required for the specified theme. If an update is required,
 * it installs the update, resets the theme data, and refreshes the current skin if it was updated.
 * Additionally, if enabled, it displays a toast notification indicating the successful update.
 *
 * @param theme The JSON object representing the theme to check for updates.
 */
const void check_updates(nlohmann::basic_json<> theme) {

	// implicitly cast the native name to string instead of using built in getters
	// for the instance that it doesn't exist
	std::string nativeName = theme["native-name"];

	// check if the update_required key exists and needs an update
	if (!theme.contains("update_required") || !theme["update_required"]) {
		return;
	}

	// install the update, hosted in the CLR
	Community::Themes->installUpdate(theme);

	/**
	 * parses the skindata of the current skin and returns object
	 *
	 * @param 1 -> checkForUpdates bool
	 * @param 2 -> setNewCommit? bool
	 * @param 3 -> native name of the theme that is affected. 
	 */
	m_Client.parseSkinData(true, true, nativeName);

	if (Settings::Get<std::string>("active-skin") == nativeName) {

		// The current selected skin was updated so refresh the skin
		steam_js_context SteamJSContext;
		SteamJSContext.reload();
	}

	// check if the user wants update messages to be displayed when a theme receives an update. 
	if (!Settings::Get<bool>("auto-update-themes-notifs")) {
		return;
	}

	std::string themeName = theme["name"].get<std::string>();
	std::string url = theme["git"]["url"].get<std::string>();


#ifdef _WIN32
	// display the windows toast 
	toastNotification("Millennium", std::format("{} was successfully updated!", themeName), url);
#endif
}

/**
 * Initializes the bootstrapper, checking for updates on installed themes and performing necessary actions.
 *
 * This method checks whether automatic theme updates are enabled. If enabled, it retrieves the path
 * to the Steam skin directory and iterates through each directory within it to parse local skin data.
 * It then attempts to fetch update information for each theme and handles any HTTP or parsing errors.
 * Finally, it checks for updates on each valid theme and performs update-related actions if necessary.
 *
 * @param setCommit A boolean indicating whether to set the new commit information.
 * @param newCommit The new commit information to set if specified.
 */
const void bootstrapper::init(bool setCommit, std::string newCommit) {

	if (Settings::Get<bool>("auto-update-themes") == false) {
		console.warn("Auto update themes is disabled, skipping theme updates.");
		return;
	}

	const std::string steamPath = config.getSkinDir();

	std::vector<nlohmann::basic_json<>> jsonBuffer;

	// check if the steam skin directory exists.
	if (!std::filesystem::exists(steamPath)) {
		return;
	}

	// iterate through each directory within the Steam skin directory.
	for (const auto& entry : std::filesystem::directory_iterator(steamPath)) {
		// check if the entry is a directory and parse local skin data.
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