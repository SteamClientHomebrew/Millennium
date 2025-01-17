#include <string>

size_t WriteFileCallback(void* contents, size_t size, size_t nMemoryBytes, void* userPtr);

size_t WriteByteCallback(char* charPtr, size_t size, size_t nMemoryBytes, std::string* data);

bool DownloadResource(const std::string& url, const std::string& outputFilePath);

const std::string Fetch(const char* url, bool shouldRetryUntilSuccess = true);