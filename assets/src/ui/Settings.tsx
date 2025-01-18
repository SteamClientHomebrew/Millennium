import { Millennium, pluginSelf, showModal } from '@steambrew/client';
import { MillenniumSettings } from '../custom_components/SettingsModal';

function RenderSettingsModal(_context: any) {
	Millennium.findElement(_context.m_popup.document, ".contextMenuItem").then((contextMenuItems: NodeListOf<Element>) => {
		for (const item of contextMenuItems) {
			// @ts-ignore
			const settingsString = LocalizationManager.LocalizeString("#Settings")

			if (item.textContent === settingsString) {

				/** Clone the settings button DOM object and edit its name and onClick */
				const millenniumContextMenu = Object.assign(item.cloneNode(true), {
					textContent: "Millennium " + settingsString,
					onclick: () => {
						showModal(<MillenniumSettings />, pluginSelf.mainWindow, {
							strTitle: "Millennium",
							popupHeight: 675,
							popupWidth: 850,
						});
					},
				})

				item.after(millenniumContextMenu);
			}
		}
	})
}

export { RenderSettingsModal }