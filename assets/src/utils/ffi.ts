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

export const PyGetSteamPath = callable<[], string>('Millennium.steam_path');
export const PyFindAllPlugins = callable<[], string>('find_all_plugins');
export const PyUpdatePluginStatus = callable<[{ pluginJson: string }], any>('ChangePluginStatus');
export const PyGetEnvironmentVar = callable<[{ variable: string }], string>('GetEnvironmentVar');
export const PyInstallTheme = callable<[{ owner: string; repo: string }], void>('themeInstaller.install_theme');
export const PyUninstallTheme = callable<[{ owner: string; repo: string }], void>('themeInstaller.uninstall_theme');
export const PyIsThemeInstalled = callable<[{ owner: string; repo: string }], boolean>('themeInstaller.check_install');
export const PyGetThemeInfo = callable<[{ repo: string; owner: string; asString: boolean }], string>('themeInstaller.get_theme_from_gitpair');
export const PyUninstallPlugin = callable<[{ pluginName: string }], boolean>('pluginInstaller.uninstall_plugin');
export const PyUpdateTheme = callable<[{ native: string }], boolean>('updater.download_theme_update');
export const PyUpdatePlugin = callable<[{ id: string; name: string }], boolean>('updater.download_plugin_update');
export const PyResyncUpdates = callable<[], string>('updater.resync_updates');
export const PyInstallPlugin = callable<[{ download_url: string; total_size: number }], void>('pluginInstaller.install_plugin');
export const PyIsPluginInstalled = callable<[{ plugin_name: string }], boolean>('pluginInstaller.check_install');
export const PySetUpdateNotificationStatus = callable<[{ status: boolean }], boolean>('updater.set_update_notifs_status');
export const PySetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>('MillenniumUpdater.set_user_wants_updates');
export const PySetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>('MillenniumUpdater.set_user_wants_update_notify');
export const PyGetActiveTheme = callable<[], string>('theme_config.get_active_theme');
export const PyFindAllThemes = callable<[], string>('find_all_themes');
export const PyGetThemeColorOptions = callable<[{ theme_name: string }], string>('theme_config.get_color_opts');
export const PyDoesThemeUseAccentColor = callable<[]>('theme_config.does_theme_use_accent_color');
export const PySetConfigKeypair = callable<[{ key: string; value: any }], void>('theme_config.set_config_keypair');
export const PySetBackendConfig = callable<[{ config: string; skip_propagation: true }], void>('config.set_all');
export const PyGetBackendConfig = callable<[], string>('config.get_all');
export const PyGetStartupConfig = callable<[], string>('GetMillenniumConfig');
export const PyGetLogData = callable<[], any[]>('GetPluginBackendLogs');
export const PySetClipboardText = callable<[{ data: string }], boolean>('SetClipboardContent');
export const PyGetRootColors = callable<[], string>('theme_config.get_colors');
export const PyChangeCondition = callable<[{ theme: string; newData: string; condition: string }], boolean>('theme_config.change_condition');
export const PyChangeColor = callable<[{ theme: string; new_color: string; color_name: string; color_type: number }], string>('theme_config.change_color');
export const PyChangeAccentColor = callable<[{ new_color: string }], string>('theme_config.change_accent_color');
export const PyUpdateMillennium = callable<[{ downloadUrl: string }], void>('MillenniumUpdater.queue_update');
