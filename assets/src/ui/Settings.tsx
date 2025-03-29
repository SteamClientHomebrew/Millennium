import { Millennium, pluginSelf, showModal } from '@steambrew/client';
import { MillenniumSettings } from '../custom_components/SettingsModal';

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
