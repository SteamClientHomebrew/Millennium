#include <filesystem>
#include <functional>
#include <string>

namespace Util
{
using ProgressCallback = std::function<void(int current, int total, const char* file)>;
bool ExtractZipArchive(const std::string& zip_path, const std::string& dest_dir, ProgressCallback progress_cb = nullptr);
} // namespace Util