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

static const constexpr std::string_view githubRepo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";

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

static float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

void Installer::installMillennium(bool silent) {

	Steam::terminateProcess();

	std::string tag_name = response["tag_name"].get<std::string>();
	std::string notes = response["body"].get<std::string>();

	totalLength = response["assets"].size();

	std::cout << "Bootstrapping Millennium: v" << tag_name << std::endl;
	std::cout << "Queued " << totalLength << " files to download" << std::endl;

	for (nlohmann::basic_json<>& item : response["assets"])
	{
		auto startTime = std::chrono::high_resolution_clock::now();

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

		double transitionDuration = 2.0;

		while (true)
		{
			auto currentTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsedTime = currentTime - startTime;

			if (elapsedTime.count() >= transitionDuration) {
				break;
			}

			float t = static_cast<float>(elapsedTime.count() / transitionDuration);
			buffer = lerp(0.0f, 1.0f, t);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		downloadIteration++;
		buffer = 0;
	}

	std::cout << std::endl;

	std::string filePath = std::format("{}/.cef-enable-remote-debugging", Steam::getInstallPath());

	if (!std::ifstream(filePath))
	{
		std::cout << "Creating steam's remote debugging flag as it wasn't previously set." << std::endl;
		std::ofstream(filePath).close();
	}

	if (!silent)
		MessageBoxA(GetForegroundWindow(), "Successfully installed Millennium! You can now start Steam", "", MB_ICONINFORMATION);
}

void Installer::uninstallMillennium() {

	Steam::terminateProcess();

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

	int notFound = 0, successfullyDeleted = 0;

	for (const auto& file : queriedFileNames)
	{
		const path filePath = path(directory) / file;

		if (exists(filePath)) {
			if (remove(filePath)) {
				std::cout << "[SUCCESS] Deleted file: " << filePath << std::endl;
				successfullyDeleted++;
			}
			else {
				std::cout << "[FATAL] Failed to delete file: " << filePath << std::endl;
			}
		}
		else {
			std::cout << "[WARNING] File does not exist: " << filePath << std::endl;
			notFound++;
		}
	}

	if (notFound == queriedFileNames.size()) {
		MessageBoxA(GetForegroundWindow(), "Millennium is already uninstalled.", "Oops!", MB_ICONINFORMATION);
	}
	if (successfullyDeleted == queriedFileNames.size()) {
		MessageBoxA(GetForegroundWindow(), "Uninstalled Millennium successfully.", "Millennium", MB_ICONINFORMATION);
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