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

import type { BackendFixtures } from "@steambrew/test";
import { backend } from "../../src/typescript/frontend/utils/ffi";
import {
    ColorType,
    MillenniumUpdateChannel,
    OnMillenniumUpdate,
    OSType,
} from "../../src/typescript/frontend/types";

export const fixtures: BackendFixtures<typeof backend> = {
    getSteamPath: {
        route: "Core_GetSteamPath",
        args: [],
        response: "/home/test/.steam/steam",
    },

    plugins: {
        getPlugins: {
            route: "Core_FindAllPlugins",
            args: [],
            response: [
                {
                    path: "/plugins/example",
                    enabled: true,
                    status: "running",
                    data: { name: "example", version: "1.0.0" },
                },
            ],
        },
        togglePlugin: {
            route: "Core_ChangePluginStatus",
            args: ['{"name":"example","enabled":true}'],
            response: undefined,
        },
        install: {
            route: "Core_InstallPlugin",
            args: ["https://example.com/plugin.zip", 1024],
            response: { success: true, started: true, opId: 1 },
        },
        isInstalled: {
            route: "Core_IsPluginInstalled",
            args: ["example"],
            response: true,
        },
        uninstall: {
            route: "Core_UninstallPlugin",
            args: ["example"],
            response: true,
        },
        update: {
            route: "Core_DownloadPluginUpdate",
            args: ["plugin-id", "plugin-name", "abc123"],
            response: { opId: 7 },
        },
        stop: {
            route: "Core_KillPluginBackend",
            args: ["example"],
            response: undefined,
        },
        getBackendLogs: {
            route: "Core_GetPluginBackendLogs",
            args: [],
            response: [{ name: "example", logs: [] }],
        },
    },

    themes: {
        activeTheme: {
            route: "Core_GetActiveTheme",
            args: [],
            response: {
                data: { name: "Default" },
                native: "Default",
                failed: false,
            },
        },
        setActiveTheme: {
            route: "Core_ChangeActiveTheme",
            args: ["Default"],
            response: undefined,
        },
        getTheme: {
            route: "Core_GetThemeFromGitPair",
            args: ["repo", "owner", false],
            response: {
                data: { name: "Default" },
                native: "Default",
                failed: false,
            },
        },
        getThemes: {
            route: "Core_FindAllThemes",
            args: [],
            response: [
                { data: { name: "Default" }, native: "Default", failed: false },
            ],
        },
        install: {
            route: "Core_InstallTheme",
            args: ["owner", "repo"],
            response: { success: true, started: true, opId: 2 },
        },
        uninstall: {
            route: "Core_UninstallTheme",
            args: ["owner", "repo"],
            response: { success: true },
        },
        isInstalled: {
            route: "Core_IsThemeInstalled",
            args: ["owner", "repo"],
            response: false,
        },
        update: {
            route: "Core_DownloadThemeUpdate",
            args: ["Default"],
            response: { opId: 3 },
        },
        colorOptions: {
            route: "Core_GetThemeColorOptions",
            args: ["Default"],
            response: [
                {
                    color: "#fff",
                    type: ColorType.Hex,
                    hex: "#ffffff",
                    defaultColor: "#000",
                },
            ],
        },
        doesUseAccentColor: {
            route: "Core_DoesThemeUseAccentColor",
            args: [],
            response: false,
        },
        getRootColors: {
            route: "Core_GetRootColors",
            args: [],
            response: ":root { --foo: red; }",
        },
        setThemeConfig: {
            route: "Core_ChangeCondition",
            args: ["Default", "{}", "Light"],
            response: { success: true },
        },
        setThemeColor: {
            route: "Core_ChangeColor",
            args: ["Default", "accent", "#abcdef", ColorType.Hex],
            response: "#abcdef",
        },
        setAccentColor: {
            route: "Core_ChangeAccentColor",
            args: ["#abcdef"],
            response: {
                accent: "#abcdef",
                accentRgb: "171,205,239",
                light1: "#fff",
                light1Rgb: "255,255,255",
                light2: "#eee",
                light2Rgb: "238,238,238",
                light3: "#ddd",
                light3Rgb: "221,221,221",
                dark1: "#444",
                dark1Rgb: "68,68,68",
                dark2: "#222",
                dark2Rgb: "34,34,34",
                dark3: "#000",
                dark3Rgb: "0,0,0",
                originalAccent: "#abcdef",
            },
        },
    },

    environment: {
        get: {
            route: "Core_GetEnvironmentVar",
            args: ["HOME"],
            response: "/home/test",
        },
    },

    config: {
        millennium: {
            getConfig: {
                route: "Core_GetBackendConfig",
                args: [],
                response: {
                    general: {
                        injectJavascript: true,
                        injectCSS: true,
                        checkForMillenniumUpdates: true,
                        checkForPluginAndThemeUpdates: true,
                        onMillenniumUpdate: OnMillenniumUpdate.NOTIFY,
                        millenniumUpdateChannel: MillenniumUpdateChannel.STABLE,
                    },
                } as any,
            },
            setConfig: {
                route: "Core_SetBackendConfig",
                args: ["{}", false],
                response: undefined,
            },
        },
        plugins: {
            getAll: {
                route: "PluginConfig_GetAll",
                args: ["example"],
                response: { someKey: "someValue" },
            },
            removeAll: {
                route: "PluginConfig_DeleteAll",
                args: ["example"],
                response: { success: true },
            },
        },
        getInitService: {
            route: "Core_GetStartConfig",
            args: [],
            response: {
                accent_color: {
                    accent: "#000",
                    accentRgb: "0,0,0",
                    light1: "#fff",
                    light1Rgb: "255,255,255",
                    light2: "#eee",
                    light2Rgb: "238,238,238",
                    light3: "#ddd",
                    light3Rgb: "221,221,221",
                    dark1: "#444",
                    dark1Rgb: "68,68,68",
                    dark2: "#222",
                    dark2Rgb: "34,34,34",
                    dark3: "#000",
                    dark3Rgb: "0,0,0",
                    originalAccent: "#000",
                },
                active_theme: {
                    data: { name: "Default" },
                    native: "Default",
                    failed: false,
                },
                conditions: {},
                settings: {
                    active: "Default",
                    scripts: true,
                    styles: true,
                    updateNotifications: true,
                },
                steamPath: "/steam",
                installPath: "/steam/install",
                themesPath: "/steam/themes",
                millenniumVersion: "0.0.0-test",
                enabledPlugins: "[]",
                hasCheckedForUpdates: false,
                updates: {},
                buildDate: "2026-01-01",
                gitCommitOid: "abcdef0",
                platformType: OSType.Linux,
                quickCss: "",
            },
        },
    },

    quickCss: {
        registerWatcher: {
            route: "Core_WatchQuickCss",
            args: [],
            response: undefined,
        },
        destroyWatcher: {
            route: "Core_UnwatchQuickCss",
            args: [],
            response: undefined,
        },
    },

    updater: {
        getUpdates: {
            route: "Core_GetUpdates",
            args: [false],
            response: { themes: [], plugins: [] },
        },
        updateMillennium: {
            route: "Core_UpdateMillennium",
            args: ["https://example.com/m.zip", 2048, true],
            response: undefined,
        },
        hasPendingMillenniumUpdateRestart: {
            route: "Core_HasPendingMillenniumUpdateRestart",
            args: [],
            response: false,
        },
    },

    crashHandler: {
        getPendingCrashes: {
            route: "Core_GetPendingCrashes",
            args: [],
            response: [{ plugin: "example", exitCode: 1 }],
        },
        acknowledgeCrash: {
            route: "Core_AcknowledgeCrash",
            args: ["example"],
            response: undefined,
        },
    },
};
