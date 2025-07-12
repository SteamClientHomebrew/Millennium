/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <direct.h>
#include <winsock2.h>
#include <windows.h> 
#include <minizip/unzip.h>
#include <iostream>
#include <filesystem>
#include <shimlogger.h>

#define WRITE_BUFFER_SIZE 8192

/**
 * @brief Check if any parent directories of a path are symbolic links.
 * @note This function is used to prevent overwriting files that are symbolic links.
 * 
 * @param path The path to check.
 * @return True if any parent directories are symbolic links, false otherwise.
 */
bool IsAnyParentSymbolicLink(std::filesystem::path path) 
{
    std::filesystem::path parentPath = path.parent_path();

    const auto IsSymlink = [](const std::filesystem::path& path) 
    {
    #ifdef _WIN32
        DWORD attributes = GetFileAttributesA(path.string().c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) 
        {
            return false;
        }
        return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; 
    #else
        struct stat path_stat;
        if (lstat(path, &path_stat) == -1) 
        {
            return false; 
        }
        return S_ISLNK(path_stat.st_mode);
    #endif
    };

    while (!parentPath.empty()) 
    {
        if (IsSymlink(parentPath)) 
        {
            return true;
        }

        if (parentPath == parentPath.parent_path())     
            break; 

        parentPath = parentPath.parent_path();
    }
    return false;
}

/**
 * @brief Create directories that do not exist.
 * @note This function is used to create directories that do not exist when extracting a zip archive.
 * 
 * @param path The path to create directories for.
 */
void CreateNonExistentDirectories(std::filesystem::path path) 
{
    std::filesystem::path dir_path(path);
    try 
    {
        std::filesystem::create_directories(dir_path);
    } 
    catch (const std::filesystem::filesystem_error& e) 
    {
        Print("Error creating directories: {}", e.what());
    }
}

/**
 * @brief Extract a zipped file to a directory.
 * @param zipfile The zip file to extract.
 * @param fileName The name of the file to extract.
 */
void ExtractZippedFile(unzFile zipfile, std::filesystem::path fileName) 
{
    if (fileName.string() == "user32.dll") 
    {
        Print("Skipping file: {}", fileName.string());
        return;
    }

    char buffer[WRITE_BUFFER_SIZE];
    FILE *outfile = fopen(fileName.string().c_str(), "wb");

    if (!outfile) 
    {
        Error("Error opening file {} for writing", fileName.string());
        return;
    }

    int nBytesRead;

    do 
    {
        nBytesRead = unzReadCurrentFile(zipfile, buffer, WRITE_BUFFER_SIZE);
        if (nBytesRead < 0) 
        {
            Error("Error reading file {} from zip archive", fileName.string());

            fclose(outfile);
            return;
        }
        if (nBytesRead > 0) 
        {
            fwrite(buffer, 1, nBytesRead, outfile);
        }
    }
    while (nBytesRead > 0);

    fclose(outfile);
}

/** 
 * @brief Check if a path is a directory path.
 * @note This function does not check if an actual directory exists, it only checks if the path string is directory-like.
*/
bool IsDirectoryPath(const std::filesystem::path& path) 
{
    return path.has_filename() == false || (path.string().back() == '/' || path.string().back() == '\\');
}

/**
 * @brief Extract a zipped archive to a directory.
 * @param zipFilePath The path to the zip file.
 * @param outputDirectory The directory to extract the zip file to.
 */
void ExtractZippedArchive(const char *zipFilePath, const char *outputDirectory) 
{
    Print("Extracting zip file: {} to {}", zipFilePath, outputDirectory);

    unzFile zipfile = unzOpen(zipFilePath);
    if (!zipfile) 
    {
        Error("Error: Cannot open zip file {}", zipFilePath);
        return;
    }

    if (unzGoToFirstFile(zipfile) != UNZ_OK) 
    {
        Error("Error: Cannot find the first file in {}", zipFilePath);
        unzClose(zipfile);
        return;
    }

    do 
    {
        char zStrFileName[256];
        unz_file_info zipedFileMetadata;

        if (unzGetCurrentFileInfo(zipfile, &zipedFileMetadata, zStrFileName, sizeof(zStrFileName), NULL, 0, NULL, 0) != UNZ_OK) 
        {
            Error("Error reading file info in zip archive");
            break;
        }

        const std::string strFileName = std::string(zStrFileName);
        auto fsOutputDirectory = std::filesystem::path(outputDirectory) / (strFileName == "user32.dll" ? "user32.queue.dll" : strFileName);

        // Skip millennium.dll to prevent overwriting development files
        if (IsDebuggerPresent() && strFileName == "millennium.dll") 
        {
            Print("Skipping millennium.dll for debugging...");
            unzCloseCurrentFile(zipfile);
            continue;
        }

        // Skip symbolic links, this is a security measure to prevent overwriting files that actually reside elsewhere
        if (IsAnyParentSymbolicLink(fsOutputDirectory)) 
        {
            Print("Skipping symbolic link: {}", fsOutputDirectory.string());
            unzCloseCurrentFile(zipfile);
            continue;   
        } 
        // Create directories if the path is a directory
        if (IsDirectoryPath(fsOutputDirectory)) 
        {
            CreateNonExistentDirectories(fsOutputDirectory);
            unzCloseCurrentFile(zipfile);
            continue;
        }

        if (unzOpenCurrentFile(zipfile) != UNZ_OK) 
        {
            Error("Error opening file {} in zip archive", zStrFileName);
            break;
        }

        ExtractZippedFile(zipfile, fsOutputDirectory.string().c_str());
        unzCloseCurrentFile(zipfile);
    } 
    while (unzGoToNextFile(zipfile) == UNZ_OK);
    unzClose(zipfile);
}