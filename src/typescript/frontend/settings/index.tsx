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

import { useEffect, useState } from 'react';
import { Classes, DialogBody, IconsModule, Navigation, SidebarNavigation, SidebarNavigationPage } from '@steambrew/client';
import { locale } from '../utils/localization-manager';
import { settingsClasses } from '../utils/classes';
import { GeneralViewModal } from './general';
import { MillenniumIcons } from '../components/Icons';
import { ThemeViewModal } from './themes';
import { PluginViewModal } from './plugins';
import { UpdatesViewModal } from './updates';
import { UpdateContextProvider } from './updates/useUpdateContext';
import { RenderLogViewer } from './logs';
import { ConfigProvider } from '../utils/config-provider';
import Styles from '../utils/styles';
import { FaPaintRoller } from 'react-icons/fa';
import { PiPlugsFill } from 'react-icons/pi';
import { QuickCssViewModal } from './quickcss';
import { useQuickAccessStore } from '../quick-access/quickAccessStore';

declare global {
	const g_PopupManager: any;
}

const tabSpotGeneral: SidebarNavigationPage = {
	visible: true,
	title: locale.settingsPanelGeneral,
	icon: <MillenniumIcons.SteamBrewLogo />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<GeneralViewModal />
		</DialogBody>
	),
	route: '/millennium/settings/general',
};

const tabSpotThemes: SidebarNavigationPage = {
	visible: true,
	title: locale.settingsPanelThemes,
	icon: <FaPaintRoller style={{ height: '20px', width: '20px' }} />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<ThemeViewModal />
		</DialogBody>
	),
	route: '/millennium/settings/themes',
};

const tabSpotQuickCSS: SidebarNavigationPage = {
	visible: true,
	title: locale.settingsPanelQuickCSS,
	icon: <MillenniumIcons.RabbitRunning />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<QuickCssViewModal />
		</DialogBody>
	),
	route: '/millennium/settings/quickcss',
};

const tabSpotPlugins: SidebarNavigationPage = {
	visible: true,
	title: locale.settingsPanelPlugins,
	icon: <PiPlugsFill style={{ height: '20px', width: '20px' }} />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<PluginViewModal />
		</DialogBody>
	),
	route: '/millennium/settings/plugins',
};

const tabSpotUpdates: SidebarNavigationPage = {
	visible: true,
	title: locale.settingsPanelUpdates,
	icon: <IconsModule.Update />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<UpdatesViewModal />
		</DialogBody>
	),
	route: '/millennium/settings/updates',
};

const tabSpotLogs: SidebarNavigationPage = {
	visible: true,
	title: locale.settingsPanelLogs,
	icon: <IconsModule.TextCodeBlock />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<RenderLogViewer />
		</DialogBody>
	),
	route: '/millennium/settings/logs',
};

const SETTINGS_TAB_KEY = 'millennium-settings-tab';
const SETTINGS_RETURN_KEY = 'millennium-return-to-settings';
const SETTINGS_ORIGINAL_START_PAGE_KEY = 'millennium-original-start-page';

/**
 * Restore the user's start_page after a JS context restart brought them back to settings.
 * Called from PluginMain on startup.
 */
export function handleSettingsReturnNavigation(): boolean {
	const shouldReturn = sessionStorage.getItem(SETTINGS_RETURN_KEY);
	if (!shouldReturn) return false;

	sessionStorage.removeItem(SETTINGS_RETURN_KEY);

	const originalStartPage = sessionStorage.getItem(SETTINGS_ORIGINAL_START_PAGE_KEY);
	if (originalStartPage !== null) {
		try {
			(window as any).settingsStore.m_ClientSettings.start_page = originalStartPage;
		} catch {}
		sessionStorage.removeItem(SETTINGS_ORIGINAL_START_PAGE_KEY);
	}

	Navigation.Navigate('/millennium/settings');
	return true;
}

export function MillenniumSettings() {
	const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup} MillenniumSettings ModalDialogPopup`;
	const settingsPages = [tabSpotGeneral, 'separator', tabSpotThemes, tabSpotQuickCSS, tabSpotPlugins, 'separator', tabSpotUpdates, tabSpotLogs];
	const [currentPage, setCurrentPage] = useState(() => sessionStorage.getItem(SETTINGS_TAB_KEY) || undefined);

	useEffect(() => {
		// Flag that we're in settings — survives RestartJSContext since cleanup won't run.
		sessionStorage.setItem(SETTINGS_RETURN_KEY, 'true');

		// Temporarily swap start_page to "library" so restarts load faster.
		try {
			const store = (window as any).settingsStore?.m_ClientSettings;
			if (store && !sessionStorage.getItem(SETTINGS_ORIGINAL_START_PAGE_KEY)) {
				sessionStorage.setItem(SETTINGS_ORIGINAL_START_PAGE_KEY, String(store.start_page));
				store.start_page = 'library';
			}
		} catch {}

		return () => {
			// Normal navigation away — clear flag and restore start_page.
			sessionStorage.removeItem(SETTINGS_RETURN_KEY);

			const saved = sessionStorage.getItem(SETTINGS_ORIGINAL_START_PAGE_KEY);
			if (saved !== null) {
				try {
					(window as any).settingsStore.m_ClientSettings.start_page = saved;
				} catch {}
				sessionStorage.removeItem(SETTINGS_ORIGINAL_START_PAGE_KEY);
			}
		};
	}, []);

	return (
		<ConfigProvider>
			<UpdateContextProvider>
				<Styles />
				{/* @ts-ignore */}
				<SidebarNavigation
					className={className}
					pages={settingsPages}
					title={locale.strMillennium}
					page={currentPage}
					onPageRequested={(page: string) => {
						setCurrentPage(page);
						sessionStorage.setItem(SETTINGS_TAB_KEY, page);
					}}
				/>
			</UpdateContextProvider>
		</ConfigProvider>
	);
}

function RenderSettingsModal(_: any, retObj: any) {
	const items = [
		{
			name: locale.strMillennium,
			onClick: () => {
				Navigation.Navigate('/millennium/settings');
			},
			visible: true,
		},
		{
			name: locale.strMillenniumLibraryManager,
			onClick: () => {
				useQuickAccessStore.getState().openQuickAccess();
			},
			visible: true,
		},
		'separator',
	];

	retObj.props.menuItems.splice(retObj.props.menuItems.length - 1, 0, ...items);
	return retObj.type(retObj.props);
}

export { RenderSettingsModal };
