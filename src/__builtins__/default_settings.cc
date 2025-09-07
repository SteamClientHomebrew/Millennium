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

#include "__builtins__/default_settings.h"

#include <cstdlib>
#include <env.h>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
#define MILLENNIUM_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define MILLENNIUM_PLATFORM_MACOS
#else
#define MILLENNIUM_PLATFORM_LINUX
#endif

using nlohmann::json;

nlohmann::json GetDefaultConfig()
{
    // Detect platform for update behavior
#ifdef MILLENNIUM_PLATFORM_WINDOWS
    OnMillenniumUpdate updateBehavior = OnMillenniumUpdate::AUTO_INSTALL;
#else
    OnMillenniumUpdate updateBehavior = OnMillenniumUpdate::NOTIFY;
#endif

    // Detect old file to determine welcome modal status
    bool hasShownWelcomeModal = std::filesystem::exists(std::filesystem::path(GetEnv("MILLENNIUM__CONFIG_PATH")) / "themes.json");

    // Build default configuration
    json default_config = {
        {"general",
         {{"injectJavascript", true},
          {"injectCSS", true},
          {"checkForMillenniumUpdates", true},
          {"checkForPluginAndThemeUpdates", true},
          {"onMillenniumUpdate", static_cast<int>(updateBehavior)},
          {"shouldShowThemePluginUpdateNotifications", true},
          {"accentColor", "DEFAULT_ACCENT_COLOR"}}                                                                           },
        {"misc",          {{"hasShownWelcomeModal", hasShownWelcomeModal}}                                                   },
        {"themes",        {{"activeTheme", "default"}, {"allowedStyles", true}, {"allowedScripts", true}}                    },
        {"notifications", {{"showNotifications", true}, {"showUpdateNotifications", true}, {"showPluginNotifications", true}}}
    };

    return default_config;
}
