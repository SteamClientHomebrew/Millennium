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

import { callable } from '@steambrew/client';

export const PyGetSteamPath = callable<[], string>('Core_GetSteamPath');
export const PyFindAllPlugins = callable<[], string>('Core_FindAllPlugins');
export const PyFindAllThemes = callable<[], string>('Core_FindAllThemes');
export const PyUpdatePluginStatus = callable<[{ pluginJson: string }], any>('Core_ChangePluginStatus');
export const PyGetEnvironmentVar = callable<[{ variable: string }], string>('Core_GetEnvironmentVar');
export const PyInstallTheme = callable<[{ owner: string; repo: string }], void>('Core_InstallTheme');
export const PyUninstallTheme = callable<[{ owner: string; repo: string }], void>('Core_UninstallTheme');
export const PyIsThemeInstalled = callable<[{ owner: string; repo: string }], boolean>('Core_IsThemeInstalled');
export const PyGetThemeInfo = callable<[{ repo: string; owner: string; asString: boolean }], string>('Core_GetThemeFromGitPair');
export const PyUninstallPlugin = callable<[{ pluginName: string }], boolean>('Core_UninstallPlugin');
export const PyUpdateTheme = callable<[{ native: string }], boolean>('Core_DownloadThemeUpdate');
export const PyUpdatePlugin = callable<[{ id: string; name: string }], boolean>('Core_DownloadPluginUpdate');
export const PyResyncUpdates = callable<[{ force: boolean }], string>('Core_GetUpdates');
export const PyInstallPlugin = callable<[{ download_url: string; total_size: number }], void>('Core_InstallPlugin');
export const PyIsPluginInstalled = callable<[{ plugin_name: string }], boolean>('Core_IsPluginInstalled');
export const PySetUpdateNotificationStatus = callable<[{ status: boolean }], boolean>('updater.set_update_notifs_status');
export const PySetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>('MillenniumUpdater.set_user_wants_updates');
export const PySetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>('MillenniumUpdater.set_user_wants_update_notify');
export const PyGetActiveTheme = callable<[], string>('Core_GetActiveTheme');
export const PyGetThemeColorOptions = callable<[{ theme_name: string }], string>('Core_GetThemeColorOptions');
export const PyDoesThemeUseAccentColor = callable<[]>('Core_DoesThemeUseAccentColor');
export const PySetBackendConfig = callable<[{ config: string; skip_propagation: true }], void>('Core_SetBackendConfig');
export const PyGetBackendConfig = callable<[], string>('Core_GetBackendConfig');
export const PyGetStartupConfig = callable<[], string>('Core_GetStartConfig');
export const PyGetLogData = callable<[], any>('Core_GetPluginBackendLogs');
export const PyGetRootColors = callable<[], string>('Core_GetRootColors');
export const PyChangeCondition = callable<[{ theme: string; newData: string; condition: string }], boolean>('Core_ChangeCondition');
export const PyChangeColor = callable<[{ theme: string; new_color: string; color_name: string; color_type: number }], string>('Core_ChangeColor');
export const PyChangeAccentColor = callable<[{ new_color: string }], string>('Core_ChangeAccentColor');
export const PyUpdateMillennium = callable<[{ downloadUrl: string; downloadSize: number; background: boolean }], void>('Core_UpdateMillennium');
