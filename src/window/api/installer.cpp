#include <window/api/installer.hpp>
#include <stdafx.h>

#include <regex>
#include <utils/clr/platform.hpp>
#include <utils/io/input-output.hpp>

namespace Community
{
	void installer::installUpdate(const nlohmann::json& skinData)
	{
		g_processingFileDrop = true;
		g_fileDropStatus = "Updating Theme...";
#ifdef _WIN32
		bool success = clr_interop::clr_base::instance().start_update(nlohmann::json({
			{"owner", skinData["github"]["owner"]},
			{"repo", skinData["github"]["repo_name"]}
		}).dump(4));

		if (success) {
			g_fileDropStatus = "Updated Successfully!";
		}
		else {
			g_fileDropStatus = "Failed to update theme...";
		}
#elif __linux__
        console.err("installer::installUpdate HAS NO IMPLEMENTATION");
#endif
        std::this_thread::sleep_for(std::chrono::seconds(2));
		g_processingFileDrop = false;
	}

    const void installer::handleFileDrop(const char* _filePath)
    {
        std::cout << "Dropped file: " << _filePath << std::endl;
        try {
            std::filesystem::path filePath(_filePath);

            if (!std::filesystem::exists(filePath) || !std::filesystem::exists(filePath / "skin.json") || !std::filesystem::is_directory(filePath))
            {
                //MsgBox("The dropped skin either doesn't exist, isn't a folder, or doesn't have a skin.json inside. "
                //    "Make sure the skin isn't archived, and it exists on your disk", "Can't Add Skin", MB_ICONERROR);
                //
                MsgBox("Can't Add Skin", [&](auto open) {

                    ImGui::TextWrapped("The dropped skin either doesn't exist, isn't a folder, or doesn't have a skin.json inside. "
                        "Make sure the skin isn't archived, and it exists on your disk");

                    if (ImGui::Button("Close")) {

                    }
                });

                return;
            }

            std::filesystem::rename(filePath, std::filesystem::path(config.getSkinDir()) / filePath.filename().string());
        }
        catch (const std::filesystem::filesystem_error& error) {
            //MsgBox(fmt::format("An error occured while adding the dropped skin to your library.\nError:\n{}", error.what()).c_str(), "Fatal Error", MB_ICONERROR);

            MsgBox("Can't Add Skin", [&](auto open) {

                ImGui::TextWrapped(fmt::format("An error occured while adding the dropped skin to your library.\nError:\n{}", error.what()).c_str());
            });
        }
    }

#ifdef _WIN32
    bool unzip(std::string zipFileName, std::string targetDirectory) {

        std::string powershellCommand = fmt::format("powershell.exe -Command \"Expand-Archive '{}' -DestinationPath '{}' -Force\"", zipFileName, targetDirectory);

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
        return false;
    }
#elif __linux__
    bool unzip(std::string zipFileName, std::string targetDirectory) {
        console.err("__linux__ unzip DOES NOT HAVE AN IMPLEMENTATION");
        return false;
    }
#endif

    const void installer::handleThemeInstall(std::string fileName, std::string downloadPath)
    {
        g_processingFileDrop = true;
        auto filePath = std::filesystem::path(config.getSkinDir()) / fileName;

        try 
        {
            g_fileDropStatus = fmt::format("Downloading {}...", fileName);

 /*           file::writeFileBytesSync(filePath, http::to_disk(downloadPath.c_str()));*/
            http::to_disk(downloadPath.c_str(), filePath.string().c_str());

            g_fileDropStatus = "Installing Theme and Verifying";

            if (unzip(filePath.string(), config.getSkinDir() + "/"))
            {
                g_fileDropStatus = "Done! Cleaning up...";
                std::this_thread::sleep_for(std::chrono::seconds(2));

                g_openSuccessPopup = true;
                m_Client.parseSkinData(false);
            }
            else {
                std::cout << "couldn't extract file" << std::endl;
                //MsgBox("couldn't extract file", "Millennium", MB_ICONERROR);

                MsgBox("Can't Add Skin", [&](auto open) {

                    ImGui::TextWrapped("couldn't extract file");

                    if (ImGui::Button("Close")) {

                    }
                });
            }
        }
        catch (const http_error&) {
            console.err("Couldn't download bytes from the file");
            //MsgBox("Couldn't download bytes from the file", "Millennium", MB_ICONERROR);

            MsgBox("Millennium", [&](auto open) {

                ImGui::TextWrapped("Couldn't download bytes from the file");

                if (ImGui::Button("Close")) {

                }
                });
        }
        catch (const std::exception& err) {
            console.err(fmt::format("Exception form {}: {}", __func__, err.what()));
            auto error = fmt::format("Exception form {}: {}", __func__, err.what()).c_str();
            //MsgBox(error, "Millennium", MB_ICONERROR);
            MsgBox("Millennium", [&](auto open) {

                ImGui::TextWrapped(error);

                if (ImGui::Button("Close")) {

                }
            });
        }
        g_processingFileDrop = false;
    }
}