import { callable, IconsModule, Millennium, pluginSelf } from '@steambrew/client';
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
import { GetUpdateCount, NotifyUpdateListeners } from './tabs/Updates';
import { formatString, locale } from './locales';

const DEFAULT_THEME_NAME = '__default__';

const ChangeTheme = callable<[{ theme_name: string }]>('cfg.change_theme');
const FindAllThemes = callable<[], string>('find_all_themes');
const FindAllPlugins = callable<[], string>('find_all_plugins');
const UpdatePluginStatus = callable<[{ pluginJson: string }], any>('update_plugin_status');
const GetRootColors = callable<[], string>('cfg.get_colors');

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

const ProcessUpdates = () => {
	if (!pluginSelf.wantsMillenniumPluginThemeUpdateNotify) {
		Logger.Log('User disabled theme & plugin update notifications, skipping...');
		return;
	}

	/** Gaben's steam account ID */
	const steamId = 76561197960287930;
	const updateCount = GetUpdateCount();
	const message = formatString(locale.themeAndPluginUpdateNotification, updateCount, updateCount > 1 ? locale.updatePlural : locale.updateSingular);

	setTimeout(() => {
		SteamClient.ClientNotifications.DisplayClientNotification(
			1,
			JSON.stringify({ title: locale.updatePanelHasUpdates, body: message, state: 'online', steamid: steamId }),
			(_: any) => {},
		);
	}, 5000);
};

const InitializePatcher = async (startTime: number, result: SettingsProps) => {
	Logger.Log(`Received props in [${(performance.now() - startTime).toFixed(3)}ms]`, result);

	const theme: ThemeItem = result.active_theme;
	const systemColors: SystemAccentColor = result.accent_color;

	ParseLocalTheme(theme);
	DispatchSystemColors(systemColors);

	const themeV1: ThemeItemV1 = result?.active_theme?.data as ThemeItemV1;

	if (themeV1?.GlobalsColors) {
		DispatchGlobalColors(themeV1?.GlobalsColors);
	}

	if (theme?.data?.hasOwnProperty('RootColors')) {
		Logger.Log('RootColors found in theme, dispatching...');
		pluginSelf.RootColors = await GetRootColors();
	}

	Object.assign(pluginSelf, {
		conditionals: result?.conditions,
		scriptsAllowed: result?.settings?.scripts ?? true,
		stylesAllowed: result?.settings?.styles ?? true,
		steamPath: result?.steamPath,
		installPath: result?.installPath,
		useInterface: result?.useInterface ?? true,
		version: result?.millenniumVersion,
		wantsMillenniumUpdates: result?.wantsUpdates ?? true,
		wantsMillenniumUpdateNotifications: result?.wantsNotify ?? true,
		wantsMillenniumPluginThemeUpdateNotify: result?.wantsThemeAndPluginNotify ?? true,
		enabledPlugins: result?.enabledPlugins ?? [],
		updates: result?.updates ?? [],
	});

	NotifyUpdateListeners();

	ProcessUpdates();
	PatchMissedDocuments();
};

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
const OnRunSteamURL = async (_: number, url: string) => {
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

	if ((tab as string) === 'devtools' && action === 'open') {
		// Open the DevTools window
		SteamClient.Browser.OpenDevTools();
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
		SteamClient.Browser.RestartJSContext();
	}

	if (tab === 'themes') {
		const themes: ThemeItem[] = JSON.parse(await FindAllThemes());
		const theme = themes.find((e) => e.native === item);
		const theme_name = !!theme && action === 'enable' ? theme.native : DEFAULT_THEME_NAME;

		ChangeTheme({ theme_name });
		SteamClient.Browser.RestartJSContext();
	}
};

// Entry point on the front end of your plugin
export default async function PluginMain() {
	console.log(IconsModule);

	const startTime = performance.now();
	const settings: SettingsProps = await Settings.FetchAllSettings();
	await InitializePatcher(startTime, settings);
	Millennium.AddWindowCreateHook(windowCreated);
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);
}
