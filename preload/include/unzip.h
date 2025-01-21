#include <filesystem>
#include <minizip/unzip.h>

#define WRITE_BUFFER_SIZE 8192

bool IsAnyParentSymbolicLink(std::filesystem::path path);

void CreateNonExistentDirectories(std::filesystem::path path);

void ExtractZippedFile(unzFile zipfile, std::filesystem::path fileName);

bool IsDirectoryPath(const std::filesystem::path& path);

void ExtractZippedArchive(const char *zipFilePath, const char *outputDirectory);