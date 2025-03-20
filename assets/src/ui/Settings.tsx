import { Millennium, pluginSelf, showModal } from '@steambrew/client';
import { MillenniumSettings } from '../custom_components/SettingsModal';

function RenderSettingsModal(_context: any) {
	Millennium.findElement(_context.m_popup.document, ".contextMenuItem").then((contextMenuItems: NodeListOf<Element>) => {
		for (const item of contextMenuItems) {
			// @ts-ignore
			if (item.textContent === LocalizationManager.LocalizeString("#Settings")) {

				/** Clone the settings button DOM object and edit its name and onClick */
				const millenniumContextMenu = Object.assign(item.cloneNode(true), {
					textContent: "Millennium",
					onclick: () => {
						showModal(<MillenniumSettings />, pluginSelf.mainWindow, {
							strTitle: "Millennium",
							popupHeight: 601,
							popupWidth: 842,
						});
					},
				})

				item.after(millenniumContextMenu);
			}
		}
	})
}

export { RenderSettingsModal }