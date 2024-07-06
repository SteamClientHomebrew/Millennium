/**
 * This file is responsible for localizing an embedded version of python. 
 */

#include <fmt/core.h>
#include <string>
#include <sys/log.h>
#include <cpr/response.h>
#include <cpr/session.h>
#include <minizip/unzip.h>
#include <filesystem>
#include <core/py_controller/co_spawn.h>
#include <fstream>

class PythonInstaller 
{
private:
    mutable std::string m_pythonZip;
    mutable std::string m_pythonDir;
    const char* m_pythonVersion = "3.11.8";

    void ExtractPython() 
    {
        Logger.Log("extracting python modules [{}]", m_pythonZip);

        unzFile zip = unzOpen(m_pythonZip.c_str());
        if (zip == nullptr) 
        {
            std::cerr << "Cannot open zip file: " << m_pythonZip << std::endl;
            return;
        }

        if (unzGoToFirstFile(zip) != UNZ_OK) 
        {
            std::cerr << "Cannot go to the first file in zip" << std::endl;
            unzClose(zip);
            return;
        }

        do 
        {
            char filename[256];
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(zip, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK) 
            {
                std::cerr << "Cannot get file info" << std::endl;
                break;
            }

            if (unzOpenCurrentFile(zip) != UNZ_OK) 
            {
                std::cerr << "Cannot open file: " << filename << std::endl;
                break;
            }

            std::vector<char> buffer(fileInfo.uncompressed_size);
            int readSize = unzReadCurrentFile(zip, buffer.data(), buffer.size());
            if (readSize < 0) 
            {
                std::cerr << "Error reading file: " << filename << std::endl;
                unzCloseCurrentFile(zip);
                break;
            }

            std::ofstream outFile(m_pythonDir + "/" + filename, std::ios::binary);
            outFile.write(buffer.data(), buffer.size());
            outFile.close();

            unzCloseCurrentFile(zip);
        }
        while (unzGoToNextFile(zip) == UNZ_OK);

        unzClose(zip);
    }

    bool IsSitePackagesEnabled() 
    {
        const auto pathPackage = pythonModulesBaseDir / "python311._pth";
        std::ifstream bufferPackage(pathPackage);

        if (!bufferPackage.is_open()) 
        {
            LOG_ERROR("failed to check if site-packages is enabled, couldn't open file [{}]", pathPackage.generic_string());
            return false;
        }

        std::string lastLine;
        std::string currentLine;

        while (std::getline(bufferPackage, currentLine)) 
        {
            lastLine = currentLine;
        }

        bufferPackage.close();
        return lastLine == "import site";
    }

    void EnableSitePackages()
    {
        if (IsSitePackagesEnabled()) 
        {
            return;
        }

        Logger.Log("enabling embedded site-packages...");

        const auto pathPackage = pythonModulesBaseDir / "python311._pth";
        std::ofstream bufferPackage(pathPackage, std::ios_base::app);
    
        if (!bufferPackage.is_open()) 
        {
            LOG_ERROR("failed to enable site-packages, couldn't open file [{}]", pathPackage.generic_string());
            return;
        }

        bufferPackage << "\nimport site" << std::endl;
        bufferPackage.close();

        if (!bufferPackage) 
        {
            LOG_ERROR("failed to enable site-packages, couldn't write to file [{}]", pathPackage.generic_string());
        }
    }

    void DownloadPython() 
    {
        std::string pythonUrl = fmt::format("https://www.python.org/ftp/python/{0}/python-{0}-embed-win32.zip", m_pythonVersion);

        cpr::Session session;
        session.SetUrl(pythonUrl);
        cpr::Response response = session.Get();

        if (response.status_code != 200) 
        {
            LOG_ERROR("failed to download Python zip: {}", response.status_code);
            return;
        }

        std::ofstream outFile(m_pythonZip, std::ios::binary);
        outFile.write(response.text.c_str(), response.text.length());
        outFile.close();

        if (std::filesystem::exists(m_pythonZip)) 
        {
            Logger.Log("python zip downloaded to [{}]", m_pythonZip);
        }
        else 
        {
            LOG_ERROR("failed to download Python zip: {}", m_pythonZip);
        }
    }

public:
    void InstallPython() 
    {
        if (!std::filesystem::exists(m_pythonDir) || !std::filesystem::is_directory(m_pythonDir) || std::filesystem::is_empty(m_pythonDir)) 
        {
            Logger.Log("python was not found, installing...");

            this->DownloadPython();
            this->ExtractPython();

            try
            {
                std::filesystem::remove(m_pythonZip);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("failed to cleanup python zip at [{}]. it is safe to manually remove. trace: {}", m_pythonZip, e.what());
            }
        }

        this->EnableSitePackages();
    }

    PythonInstaller(std::string pythonVersion)
    {
        try
        {
            std::filesystem::create_directories(pythonModulesBaseDir);

            this->m_pythonZip = (pythonModulesBaseDir / fmt::format("python-{0}-embed.zip", pythonVersion)).generic_string();
            this->m_pythonDir = pythonModulesBaseDir.generic_string();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("PythonInstaller failed to initialize: {}", e.what());
        }
    }
};
