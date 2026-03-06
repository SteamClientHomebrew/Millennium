/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include "millennium/backend_init.h"
#ifdef _WIN32
#include "millennium/plat_msg.h"
#include "millennium/types.h"
#include "millennium/filesystem.h"
#include "millennium/environment.h"
#endif

/**
 * Restores the original `SharedJSContext` by renaming the backup file to the original file.
 * It reverses the patches done in the preloader module
 *
 * @note this function is only applicable to Windows
 */
void backend_initializer::compat_restore_shared_js_context()
{
#ifdef _WIN32
    logger.log("Restoring SharedJSContext...");

    const auto SteamUIModulePath = platform::get_steam_path() / "steamui" / "index.html";

    /** if the sequence isn't found, it indicates it hasn't been patched by millennium <= 2.30.0 preloader */
    if (platform::read_file(SteamUIModulePath.string()).find("<!doctype html><html><head><title>SharedJSContext</title></head></html>") == std::string::npos) {
        logger.log("SharedJSContext isn't patched, skipping...");
        return;
    }

    const auto librariesPath = platform::get_steam_path() / "steamui" / "libraries";
    std::string libraryChunkJS;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(librariesPath)) {
            if (entry.is_regular_file() && entry.path().filename().string().substr(0, 10) == "libraries~" && entry.path().extension() == ".js") {
                libraryChunkJS = entry.path().filename().string();
                break;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        logger.warn("Failed to find libraries~xxx.js: {}", e.what());
    }

    if (libraryChunkJS.empty()) {
        MessageBoxA(NULL,
                    "Millennium failed to find a key library used by Steam. "
                    "Let our developers know if you see this message, it's likely a bug.\n"
                    "You can reach us over at steambrew.app/discord",
                    "Millennium", MB_ICONERROR);

        return;
    }

    std::string fileContent = fmt::format(
        R"(<!doctype html><html style="width: 100%; height: 100%"><head><title>SharedJSContext</title><meta charset="utf-8"><script defer="defer" src="/libraries/{}"></script><script defer="defer" src="/library.js"></script><link href="/css/library.css" rel="stylesheet"></head><body style="width: 100%; height: 100%; margin: 0; overflow: hidden;"><div id="root" style="height:100%; width: 100%"></div><div style="display:none"></div></body></html>)",
        libraryChunkJS);

    try {
        platform::write_file(SteamUIModulePath.string(), fileContent);
    } catch (const std::system_error& e) {
        logger.warn("Failed to restore SharedJSContext: {}", e.what());
    } catch (const std::exception& e) {
        logger.warn("Failed to restore SharedJSContext: {}", e.what());
    }

    logger.log("Restored SharedJSContext...");
#endif
}
