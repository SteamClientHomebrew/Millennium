#include "deps.h"
#include <utilities/http.h>
#include <generic/stream_parser.h>
#include <minizip/unzip.h>
#include <iostream>
#include <fstream>
#include <utilities/log.hpp>
#include <boxer/boxer.h>
#include <generic/base.h>

namespace dependencies 
{
    class extract_except : public std::exception {
    private:
        std::string message_;

    public:
        extract_except(const std::string& message) : message_(message) {}

        // Override what() to provide exception message
        const char* what() const noexcept override {
            return message_.c_str();
        }
    };

    bool extract_package(std::filesystem::path path) 
    {
        unzFile zipFile = unzOpen(path.generic_string().c_str());
        if (zipFile == nullptr) {
            throw extract_except(fmt::format("Failed to open zip file -> {}", path.generic_string()));
        }

        int err = unzGoToFirstFile(zipFile);
        if (err != UNZ_OK) {
            unzClose(zipFile);
            throw extract_except("Failed to go to first file in zip file");
        }

        do {
            char filename[256];
            unz_file_info fileInfo;
            err = unzGetCurrentFileInfo(zipFile, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0);
            if (err != UNZ_OK) {
                throw extract_except("Failed to get file info from current extracting file");
            }

            std::string extract_path = path.parent_path().string() + "/" + std::string(filename);
            // console.log("Extracing python bin -> {}", extract_path);

            if (unzOpenCurrentFile(zipFile) != UNZ_OK) {
                throw extract_except("Failed to open current file!");
            }

            FILE* outFile = fopen((const char*)extract_path.c_str(), "wb");
            if (outFile == nullptr) {
                unzCloseCurrentFile(zipFile);
                throw extract_except("Failed to open output file to write extracted data!");
            }

            char buffer[4096];
            int bytesRead;
            do {
                bytesRead = unzReadCurrentFile(zipFile, buffer, sizeof(buffer));
                if (bytesRead < 0) {
                    fclose(outFile);
                    unzCloseCurrentFile(zipFile);
                    throw extract_except("Failed to read from current file.");
                }
                if (bytesRead > 0)
                    fwrite(buffer, 1, bytesRead, outFile);
            } while (bytesRead > 0);

            fclose(outFile);
        } 
        while (unzGoToNextFile(zipFile) == UNZ_OK);

        unzClose(zipFile);
        return true;
    }

    bool is_installed(const std::filesystem::path& directory) {
        if (std::filesystem::exists(directory) || std::filesystem::is_directory(directory)) {
            // If the directory doesn't exist or it's not a directory
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                // Check if there's any entry in the directory
                return true;
            }
            return false;
        }
        return false;
    }

    /// @brief plugins rely on python. millennium embeds the python interpretor into its main DLL, however python still needs base runtime libraries in __builtins__
    /// @return 
    bool embed_python() 
    {
        console.head("python pkg [win32]");

        if (is_installed(python_bootzip.parent_path())) {     
            console.log_item("status", "python seems to be ready...", true);
            return true;
        }

        // get python embedd package from ftp
        const char *url = "https://www.python.org/ftp/python/3.11.8/python-3.11.8-embed-win32.zip";

        try {
		    std::filesystem::create_directories(python_bootzip.parent_path());
        }
        catch (const std::exception& e) {
            console.err("error creating python directories -> {}", e.what());

            boxer::show(fmt::format("An error occured creating path [{}], try creating it yourself\n\nTrace:\n{}", python_bootzip.parent_path().string(), e.what()).c_str(), "Whoops!", boxer::Style::Error);
        }

        console.log("downloading python 3.11.8 win32...");
        if (download_file(url, python_bootzip.generic_string().c_str()) != 0) {
            console.err("Failed to download python embed pacakge.");

            boxer::show(fmt::format("Failed to download Python embed runtime! "
            "If you have a valid internet connection and you continue to receieve this message please make an issue on the github." 
            "\n\nYou can manually download the binaries from:\n{}\nand extract it here:\n{}\n\n"
            "Millennium is going to continue to run as it, though it will most likely not function properly", url, python_bootzip.parent_path().string()).c_str(), "Fatal Startup Error", boxer::Style::Error);

            return false;
        } 

        bool result = false;

        try {
            console.log("extracting module packages...");
            result = extract_package(python_bootzip);
        }
        catch (const extract_except& e) {
            boxer::show(fmt::format("A potentially fatal error occured when trying to extract python.\n\n"
            "Stack Trace:\n{}\n\nCreate an issue on our GitHub if you continue to face this!", e.what()).c_str(), "Oops! Something went wrong", boxer::Style::Error);
        }

        //boxer::show("Millennium had to do some background for the first time launch. Its recommended you restart steam just to make sure changes take affect!", "Important Tip!");
        return result;
    }
}