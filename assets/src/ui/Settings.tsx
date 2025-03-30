import { Millennium, pluginSelf, showModal } from '@steambrew/client';
import { MillenniumSettings } from '../custom_components/SettingsModal';
import { locale } from '../locales';
import { pagedSettingsClasses } from '../classes';
import { Logger } from '../Logger';

declare global {
	const g_PopupManager: any;
}

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

	Logger.Log(`OpenSettingsTab( '${activeTab}' )`);

	// shitty routing replacement
	const tabs = (await Millennium.findElement(
		popup.m_popup.document,
		`.${pagedSettingsClasses.PagedSettingsDialog_PageListItem}`,
	)) as NodeListOf<HTMLElement>;
	for (const tab of tabs) {
		if (tab.textContent === settingsTabsMap[activeTab]) {
			tab.click();
			break;
		}
	}
}

function ShowSettingsModal() {
	showModal(<MillenniumSettings />, pluginSelf.mainWindow, {
		strTitle: 'Millennium',
		popupHeight: 601,
		popupWidth: 842,
	});
}

function RenderSettingsModal(_context: any) {
	Millennium.findElement(_context.m_popup.document, '.contextMenuItem').then((contextMenuItems: NodeListOf<Element>) => {
		/** Get second last context menu item */
		const settingsItem = contextMenuItems[contextMenuItems.length - 2] as HTMLElement;

		/** Clone the settings button DOM object and edit its name and onClick */
		const millenniumContextMenu = Object.assign(settingsItem.cloneNode(true), {
			textContent: 'Millennium',
			onclick: () => {
				ShowSettingsModal();
			},
		});

		settingsItem.after(millenniumContextMenu);
	});
}

export { ShowSettingsModal, RenderSettingsModal };
