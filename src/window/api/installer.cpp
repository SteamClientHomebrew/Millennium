#include <window/api/installer.hpp>
#include <stdafx.h>

#include <regex>

namespace Community
{
	void installer::writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
	{
		console.log(std::format("writing file to: {}", filePath.string()));

		std::ofstream fileStream(filePath, std::ios::binary);
		if (!fileStream)
		{
			console.log(std::format("Failed to open file for writing: {}", filePath.string()));
			return;
		}

		fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

		if (!fileStream)
		{
			console.log(std::format("Error writing to file: {}", filePath.string()));
		}

		fileStream.close();
	}

	bool unzip(std::string zipFileName, std::string targetDirectory) {

		const std::string powershellCommand = 
			std::format("powershell.exe -Command \"Expand-Archive '{}' -DestinationPath '{}' -Force\"", zipFileName, targetDirectory);

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		if (CreateProcess(NULL, const_cast<char*>(powershellCommand.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);

			return true;
		}
		else {
			return false;
		}
	}

	void installer::installUpdate(const nlohmann::json& skinData)
	{
		const std::string nativeName = skinData["native-name"];
		std::cout << skinData.dump(4) << std::endl;

		g_processingFileDrop = true;

		auto filePath = std::filesystem::path(config.getSkinDir()) / std::format("{}.zip", nativeName);

		try {

			try {
				std::string message = nlohmann::json({
					{
						{"owner", skinData["github"]["owner"]},
						{"repo", skinData["github"]["repo_name"]}
					}
				}).dump(4);

				std::cout << message << std::endl;

				const auto response = http::post("/api_v2/api_v2/get-download", message);

				std::cout << response << std::endl;
			}
			catch (nlohmann::detail::exception&) {
				console.err("Couldn't send download count to server, error getting github keys from json");
			}
			catch (const http_error&) {
				console.err("Couldn't send download count to server");
			}

			g_fileDropStatus = std::format("Downloading {}...", std::format("{}.zip", nativeName));
			writeFileBytesSync(filePath, http::get_bytes(skinData["git"]["download"].get<std::string>().c_str()));

			g_fileDropStatus = "Processing Theme Information...";

			std::string zipFilePath = filePath.string();
			std::string destinationFolder = config.getSkinDir() + "/";

			g_fileDropStatus = "Installing Theme...";

			if (unzip(zipFilePath, destinationFolder)) {
				g_fileDropStatus = "Done!";
				g_openSuccessPopup = true;
			}
			else {
				MessageBoxA(GetForegroundWindow(), "couldn't extract file", "Millennium", MB_ICONERROR);
			}

		}
		catch (const http_error&) {
			console.err("Couldn't download bytes from the file");
			MessageBoxA(GetForegroundWindow(), "Couldn't download bytes from the file", "Millennium", MB_ICONERROR);
		}
		catch (const std::exception& err) {
			console.err(std::format("Exception form {}: {}", __func__, err.what()));
			MessageBoxA(GetForegroundWindow(), std::format("Exception form {}: {}", __func__, err.what()).c_str(), "Millennium", MB_ICONERROR);
		}
		g_processingFileDrop = false;
	}
}