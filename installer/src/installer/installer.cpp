#include <src/installer/installer.hpp>
#include <nlohmann/json.hpp>
#include <utils/http/http_client.hpp>
#include <utils/steam/steam_helpers.hpp>

#include <iostream>
#include <chrono>
#include <windows.h>
#include <wininet.h>

//interact with disk when uninstalling and installing
#include <filesystem>
#include <fstream>

static 
const 
constexpr 
std::string_view githubRepo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";

bool Installer::downloadFile(std::string fileUrl) {

	std::cout << "     Getting [" << currentFileName << "]" << std::endl;

	HINTERNET connection = InternetOpenA("millennium.installer", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	HINTERNET hConnection = InternetOpenUrlA(connection, fileUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

	std::string path = Steam::getInstallPath() + "/" + currentFileName;

	std::cout << "     Touching [" << path << "]" << std::endl;

	std::remove(path.c_str());

	HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	DWORD bytesRead;
	BYTE buffer[1024];
	while (InternetReadFile(hConnection, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		DWORD bytesWritten;
		WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL);
	}

	CloseHandle(hFile);

	return true;
}

void Installer::installMillennium() {

	if (Steam::isRunning()) {
		std::cout << "Steam is running. close Steam and retry..." << std::endl;
		return;
	}

	nlohmann::json response = nlohmann::json::parse(http::get(githubRepo.data()));

	if (response.contains("message") && response["message"].get<std::string>() == "Not Found")
	{
		std::cout << "Couldn't get latest release because it doesn't exist. Try again later?" << std::endl;
		return;
	}

	std::string tag_name = response["tag_name"].get<std::string>();
	std::string notes = response["body"].get<std::string>();

	totalLength = response["assets"].size();

	std::cout << "Bootstrapping Millennium: v" << tag_name << std::endl;
	std::cout << "Queued " << totalLength << " files to download" << std::endl;

	for (nlohmann::basic_json<>& item : response["assets"])
	{
		auto startTime = std::chrono::high_resolution_clock::now();

		downloadIteration++;
		currentFileName = item["name"].get<std::string>();

		std::string download = item["browser_download_url"].get<std::string>();

		double size = static_cast<double>(item["size"].get<int>()) / 1048576.0;
		int download_count = item["download_count"].get<int>();

		std::cout << std::endl;

		std::cout << "  File size: " << size << "mb" << std::endl;
		std::cout << "  Download Count: " << download_count << std::endl;

		downloadFile(download);

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

		std::cout << "\n  [+] " 
			<< std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() 
			<< " milliseconds elapsed" << std::endl;
	}

	std::cout << std::endl;

	std::string filePath = std::format("{}/.cef-enable-remote-debugging", Steam::getInstallPath());

	if (!std::ifstream(filePath))
	{
		std::cout << "Creating steam's remote debugging flag as it wasn't previously set." << std::endl;
		std::ofstream(filePath).close();
	}

	std::cout << "Installation completed. You can now start Steam" << std::endl;
}

void Installer::uninstallMillennium() {
	nlohmann::json response = nlohmann::json::parse(http::get(githubRepo.data()));

	std::vector<std::string> queriedFileNames;

	//get the file name assets.
	for (const auto& asset : response["assets"]) {
		if (asset.find("name") != asset.end()) {
			queriedFileNames.push_back(asset["name"]);
		}
	}

	if (Steam::isRunning()) {
		std::cout << "Steam is running. close Steam and retry..." << std::endl;
		return;
	}

	std::string directory = Steam::getInstallPath();

	using namespace std::filesystem;

	for (const auto& file : queriedFileNames)
	{
		const path filePath = path(directory) / file;

		if (exists(filePath)) {
			if (remove(filePath)) {
				std::cout << "[SUCCESS] Deleted file: " << filePath << std::endl;
			}
			else {
				std::cout << "[FATAL] Failed to delete file: " << filePath << std::endl;
			}
		}
		else {
			std::cout << "[WARNING] File does not exist: " << filePath << std::endl;
		}
	}
}

void Installer::resetMillennium() {

	if (Steam::isRunning()) {
		return;
	}

	using namespace std::filesystem;
	path skinsFolderPath = path(Steam::getInstallPath()) / "steamui" / "skins";

	try {
		std::filesystem::remove_all(skinsFolderPath);
		std::cout << "[SUCESS] Successfully reset the millennium client" << std::endl;
	}
	catch (std::filesystem::filesystem_error& e) {
		std::cout << "[FATAL] Failed to reset the millennium client" << e.what() << std::endl;
	}

	const char* keyPath = "Software\\Millennium";
	LONG result = RegDeleteTreeA(HKEY_CURRENT_USER, keyPath);

	if (result == ERROR_SUCCESS) {
		std::cout << "Registry key '" << keyPath << "' and its subkeys have been deleted" << std::endl;
	}
	else if (result == ERROR_FILE_NOT_FOUND) {
		std::cout << "Registry key '" << keyPath << "' was not found" << std::endl;
	}
	else {
		std::cout << "Error deleting registry key '" << keyPath << "' Error code: " << result << std::endl;;
	}
}