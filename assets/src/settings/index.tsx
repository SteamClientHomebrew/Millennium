import {
	Classes,
	DialogBody,
	IconsModule,
	Millennium,
	ModalPosition,
	pluginSelf,
	showModal,
	ShowModalResult,
	SidebarNavigation,
	SidebarNavigationPage,
} from '@steambrew/client';
import { locale } from '../../locales';
import { pagedSettingsClasses, settingsClasses } from '../utils/classes';
import { GeneralViewModal } from './general';
import { MillenniumIcons } from '../components/Icons';
import { ThemeViewModal } from './themes';
import { PluginViewModal } from './plugins';
import { RenderUpdatesSettingsTab, UpdatesViewModal } from './updates';
import { UpdateContextProvider } from './updates/useUpdateContext';
import { RenderLogViewer } from './logs';
import { useEffect } from 'react';
import { ConfigProvider } from '../config-provider';
import Styles from '../utils/styles';

declare global {
	const g_PopupManager: any;
}

const tabSpotGeneral = {
	visible: true,
	title: locale.settingsPanelGeneral,
	icon: <IconsModule.Settings />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<GeneralViewModal />
		</DialogBody>
	),
};

const tabSpotSettings = {
	visible: true,
	title: locale.settingsPanelThemes,
	icon: <MillenniumIcons.Themes />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<ThemeViewModal />
		</DialogBody>
	),
};

const tabSpotPlugins = {
	visible: true,
	title: locale.settingsPanelPlugins,
	icon: <MillenniumIcons.Plugins />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<PluginViewModal />
		</DialogBody>
	),
};

const tabSpotUpdates = {
	visible: true,
	title: <RenderUpdatesSettingsTab />,
	icon: <IconsModule.Update />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<UpdateContextProvider>
				<UpdatesViewModal />
			</UpdateContextProvider>
		</DialogBody>
	),
};

const tabSpotLogs = {
	visible: true,
	title: locale.settingsPanelLogs,
	icon: <IconsModule.TextCodeBlock />,
	content: (
		<DialogBody className={Classes.SettingsDialogBodyFade}>
			<RenderLogViewer />
		</DialogBody>
	),
};

const settingsTabsMap = {
	themes: locale.settingsPanelThemes,
	plugins: locale.settingsPanelPlugins,
	updates: locale.settingsPanelUpdates,
	report: locale.settingsPanelBugReport,
	logs: locale.settingsPanelLogs,
	settings: locale.settingsPanelSettings,
};

export type SettingsTabs = keyof typeof settingsTabsMap;

export async function OpenSettingsTab(popup: any, activeTab: SettingsTabs) {
	if (!activeTab) {
		return;
	}

	/** FIXME: Fix this to use actual router, tried it and it worked but it broke other portions of the UI. */
	const tabs = (await Millennium.findElement(popup.m_popup.document, `.${pagedSettingsClasses.PagedSettingsDialog_PageListItem}`)) as NodeListOf<HTMLElement>;
	for (const tab of tabs) {
		if (tab.textContent.includes(settingsTabsMap[activeTab])) {
			tab.click();
			break;
		}
	}
}

export function MillenniumSettings({ popup }: { popup: ShowModalResult }) {
	const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup} MillenniumSettings`;
	const pages: SidebarNavigationPage[] = [tabSpotGeneral, tabSpotSettings, tabSpotPlugins, tabSpotUpdates, tabSpotLogs];

	/**
	 * For some reason, putting a sidebar navigation inside a modal causes the modal to not close when clicking outside of it.
	 * This is a workaround to fix that.
	 */
	useEffect(() => {
		const handleClick = (event: MouseEvent) => {
			if ((event.target as HTMLElement).className === 'ModalPosition') {
				popup.Close();
			}
		};

		pluginSelf.mainWindow.document.addEventListener('click', handleClick);

		return () => {
			pluginSelf.mainWindow.document.removeEventListener('click', handleClick);
		};
	}, []);

	return (
		<ConfigProvider>
			<ModalPosition>
				<Styles />
				{/* @ts-ignore */}
				<SidebarNavigation className={className} pages={pages} title={'Millennium'} />
			</ModalPosition>
		</ConfigProvider>
	);
}

function RenderSettingsModal(_: any, retObj: any) {
	const index = retObj.props.menuItems.findIndex((prop: any) => prop.name === '#Menu_Settings');

	if (index !== -1) {
		retObj.props.menuItems.splice(index + 1, 0, {
			name: 'Millennium',
			onClick: () => {
				let modal: any = {};
				Object.assign(
					modal,
					showModal(<MillenniumSettings popup={modal} />, pluginSelf.mainWindow, {
						bNeverPopOut: true,
					}),
				);
			},
			visible: true,
		});
	}

	return retObj.type(retObj.props);
}

export { RenderSettingsModal };
