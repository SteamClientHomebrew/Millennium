import { callable, Millennium, pluginSelf } from '@steambrew/client';
import { PatchDocumentContext } from './patcher/index';
import { OpenSettingsTab, RenderSettingsModal, SettingsTabs, ShowSettingsModal } from './ui/Settings';
import { ThemeItem, SystemAccentColor, SettingsProps, ThemeItemV1, PluginComponent } from './types';
import { DispatchSystemColors } from './patcher/SystemColors';
import { ParseLocalTheme } from './patcher/ThemeParser';
import { Logger } from './Logger';
import { PatchNotification } from './ui/Notifications';
import { Settings } from './Settings';
import { DispatchGlobalColors } from './patcher/v1/GlobalColors';
import { ShowUpdaterModal } from './custom_components/UpdaterModal';
import { ShowWelcomeModal } from './custom_components/modals/WelcomeModal';

const PatchMissedDocuments = () => {
	// @ts-ignore
	g_PopupManager?.m_mapPopups?.data_?.forEach((element: any) => {
		if (element?.value_?.m_popup?.window?.HAS_INJECTED_THEME === undefined) {
			PatchDocumentContext(element?.value_);
		}
	});

	// @ts-ignore
	if (g_PopupManager?.m_mapPopups?.data_?.length === 0) {
		Logger.Warn('windowCreated callback called, but no popups found...');
	}
};

const windowCreated = (windowContext: any): void => {
	const windowName = windowContext.m_strName;
	const windowTitle = windowContext.m_strTitle;

	if (windowTitle === 'Steam Root Menu') {
		pluginSelf.useInterface && RenderSettingsModal(windowContext);
	}

	if (windowTitle.includes('notificationtoasts')) {
		PatchNotification(windowContext.m_popup.document);
	}

	pluginSelf.windows ??= {};

	Object.assign(pluginSelf.windows, {
		[windowName]: windowContext.m_popup.window,
		...(windowName !== windowTitle && { [windowTitle]: windowContext.m_popup.window }),
	});

	// @ts-ignore
	g_PopupManager?.m_mapPopups?.data_?.forEach((element: any) => {
		if (element?.value_?.m_strName === 'SP Desktop_uid0') {
			pluginSelf.mainWindow = element?.value_?.m_popup?.window;

			if (pluginSelf.mainWindow?.HAS_SHOWN_UPDATER === undefined) {
				setTimeout(() => ShowWelcomeModal().then(ShowUpdaterModal.bind(null)), 1000);
				pluginSelf.mainWindow.HAS_SHOWN_UPDATER = true;
			}
		}
	});

	PatchMissedDocuments();
	PatchDocumentContext(windowContext);
};

const InitializePatcher = (startTime: number, result: SettingsProps) => {
	Logger.Log(`Received props in [${(performance.now() - startTime).toFixed(3)}ms]`, result);

	const theme: ThemeItem = result.active_theme;
	const systemColors: SystemAccentColor = result.accent_color;

	ParseLocalTheme(theme);
	DispatchSystemColors(systemColors);

	const themeV1: ThemeItemV1 = result?.active_theme?.data as ThemeItemV1;

	if (themeV1?.GlobalsColors) {
		DispatchGlobalColors(themeV1?.GlobalsColors);
	}

	Object.assign(pluginSelf, {
		systemColors,
		conditionals: result?.conditions,
		scriptsAllowed: result?.settings?.scripts ?? true,
		stylesAllowed: result?.settings?.styles ?? true,
		steamPath: result?.steamPath,
		installPath: result?.installPath,
		useInterface: result?.useInterface ?? true,
		version: result?.millenniumVersion,
		wantsMillenniumUpdates: result?.wantsUpdates ?? true,
		wantsMillenniumUpdateNotifications: result?.wantsNotify ?? true,
		enabledPlugins: result?.enabledPlugins ?? [],
	});

	PatchMissedDocuments();
};

const DEFAULT_THEME_NAME = '__default__';

// Duplicating from tabs/Themes.tsx... import them later
const ChangeTheme = callable<[{ theme_name: string }]>('cfg.change_theme');
const FindAllThemes = callable<[], string>('find_all_themes');
// Duplicating from tabs/Plugins.tsx... import them later
const FindAllPlugins = callable<[], string>('find_all_plugins');
const UpdatePluginStatus = callable<[{ pluginJson: string }], any>('update_plugin_status');

function RestartJSContext() {
	SteamClient.Browser.RestartJSContext();
}

/**
 * steam://millennium URL support.
 *
 * Example:
 * "steam://millennium" -> Open Millennium dialog
 * "steam://millennium/updates" -> Open the "Updates" tab
 * "steam://millennium/themes/disable" -> Use default theme
 * "steam://millennium/themes/enable/aerothemesteam" -> Enable the Office 2007 theme using its internal name
 * "steam://millennium/plugins/disable/steam-db" -> Disable the SteamDB plugin using its internal name
 * "steam://millennium/plugins/disable" -> Disable all plugins
 */
async function OnRunSteamURL(_: number, url: string) {
	const [tab, action, item] = url.split('/').slice(3) as [SettingsTabs, string, string];
	Logger.Log('OnRunSteamURL: Executing %o', url);

	if (!action) {
		const callback = (popup: any) => {
			if (popup.title !== 'Millennium') {
				return;
			}

			// It's async, but it's not used (or needed) here since
			// PopupManager.AddPopupCreatedCallback doesn't support it.
			OpenSettingsTab(popup, tab);
		};

		if (!g_PopupManager.m_rgPopupCreatedCallbacks.some((e: any) => e === callback)) {
			Millennium.AddWindowCreateHook(callback);
		}
		ShowSettingsModal();
		return;
	}

	if (tab === 'plugins') {
		// God, why
		const plugins: PluginComponent[] = JSON.parse(await FindAllPlugins()).map((e: PluginComponent) => ({ ...e, plugin_name: e.data.name }));
		if (item) {
			if (!plugins.some((e) => e.data.name === item)) {
				return;
			}

			const neededPlugin = plugins.find((e) => e.data.name === item);
			neededPlugin.enabled = action === 'enable';
		} else {
			// Disable them all
			for (const plugin of plugins) {
				// ..except me, of course
				plugin.enabled = plugin.data.name === 'core';
			}
		}

		UpdatePluginStatus({ pluginJson: JSON.stringify(plugins) });
		RestartJSContext();
	}

	if (tab === 'themes') {
		const themes: ThemeItem[] = JSON.parse(await FindAllThemes());
		const theme = themes.find((e) => e.native === item);
		const theme_name = !!theme && action === 'enable' ? theme.native : DEFAULT_THEME_NAME;

		ChangeTheme({ theme_name });
		RestartJSContext();
	}
}

// Entry point on the front end of your plugin
export default async function PluginMain() {
	const startTime = performance.now();
	const settings: SettingsProps = await Settings.FetchAllSettings();
	InitializePatcher(startTime, settings);
	Millennium.AddWindowCreateHook(windowCreated);
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);
}
