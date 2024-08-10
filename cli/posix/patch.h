#include <sys/locals.h>
#include <regex>
#include <util/log.h>

bool IsGitInstalled() {
    return system("git --version >nul 2>&1") == 0;
}

char GetFirstCharacter(const std::string& str) {
    auto it = std::find_if(str.begin(), str.end(), [](char ch) {
        return !std::isspace(static_cast<unsigned char>(ch));
    });

    if (it != str.end()) 
        return *it;
    
    return '\0';
}

bool CopyTextFile(const std::string& sourcePath, const std::string& destPath) {
    std::ifstream src(sourcePath, std::ios::binary);
    if (!src) {
        LOG_FAIL("Failed to open source file: " << sourcePath);
        return false;
    }

    std::ofstream dest(destPath, std::ios::binary);
    if (!dest) {
        LOG_FAIL("Failed to open destination file: " << destPath);
        return false;
    }

    dest << src.rdbuf(); // Copy the content of source to destination
    return true;
}

int PatchPosixStartScript() {
    const std::string steamScriptPath = (SystemIO::GetSteamPath() / "steam.sh").string();
    const std::string steamScriptOutputPath = (SystemIO::GetSteamPath() / "steam-orig.sh").string();

    // read file steam.sh
    std::string injectShim = "bash ~/.millennium/start.sh\nexit 0";
    std::string steamScript = SystemIO::ReadFileSync(steamScriptPath);

    const size_t scriptSize = steamScript.size();

    if (steamScript.find(injectShim) != std::string::npos) {
        LOG_INFO("Steam already patched, proceeding with application...");
        return 0;
    }

    steamScript = fmt::format("{}{}", injectShim, std::string(scriptSize - injectShim.size(), ' '));

    if (steamScript.size() != scriptSize) {
        LOG_FAIL("Failed to patch steam.sh");
        return 1;
    }

    CopyTextFile(steamScriptPath, steamScriptOutputPath);
    SystemIO::WriteFileSync(steamScriptPath, steamScript);

    LOG_INFO("Successfully patched steam!");
    return 0;
}