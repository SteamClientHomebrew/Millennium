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

import { Classes, DialogBody, IconsModule, Navigation, SidebarNavigation, SidebarNavigationPage } from '@steambrew/client';
import { locale } from '../../locales';
import { settingsClasses } from '../utils/classes';
import { GeneralViewModal } from './general';
import { MillenniumIcons } from '../components/Icons';
import { ThemeViewModal } from './themes';
import { PluginViewModal } from './plugins';
import { UpdatesViewModal } from './updates';
import { UpdateContextProvider } from './updates/useUpdateContext';
import { RenderLogViewer } from './logs';
import { ConfigProvider } from '../config-provider';
import Styles from '../utils/styles';
import { FaPaintRoller } from 'react-icons/fa';
import { PiPlugsFill } from 'react-icons/pi';

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
			<UpdateContextProvider>
				<UpdatesViewModal />
			</UpdateContextProvider>
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

export function MillenniumSettings() {
	const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup} MillenniumSettings ModalDialogPopup`;
	const settingsPages = [tabSpotGeneral, 'separator', tabSpotThemes, tabSpotPlugins, 'separator', tabSpotUpdates, tabSpotLogs];

	return (
		<ConfigProvider>
			<Styles />
			{/* @ts-ignore */}
			<SidebarNavigation className={className} pages={settingsPages} title={'Millennium'} />
		</ConfigProvider>
	);
}

function RenderSettingsModal(_: any, retObj: any) {
	const index = retObj.props.menuItems.findIndex((prop: any) => prop.name === '#Menu_Settings');

	if (index !== -1) {
		retObj.props.menuItems.splice(index + 1, 0, {
			name: 'Millennium',
			onClick: () => {
				Navigation.Navigate('/millennium/settings');
			},
			visible: true,
		});
	}

	return retObj.type(retObj.props);
}

export { RenderSettingsModal };
