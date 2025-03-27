import { Millennium, pluginSelf } from '@steambrew/client';
import { PatchDocumentContext } from './patcher/index';
import { RenderSettingsModal } from './ui/Settings';
import { ThemeItem, SystemAccentColor, SettingsProps, ThemeItemV1 } from './types';
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

// Entry point on the front end of your plugin
export default async function PluginMain() {
	const startTime = performance.now();
	const settings: SettingsProps = await Settings.FetchAllSettings();
	InitializePatcher(startTime, settings);
	Millennium.AddWindowCreateHook(windowCreated);
}
