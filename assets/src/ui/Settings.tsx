import React, { useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import { PluginViewModal } from '../tabs/Plugins'
import { ThemeViewModal } from '../tabs/Themes'
import { UpdatesViewModal } from '../tabs/Updates'
import { IconsModule, Millennium, pluginSelf, Classes } from '@millennium/ui';
import { locale } from '../locales';
import { pagedSettingsClasses } from "../classes";
import * as CustomIcons from '../custom_components/CustomIcons'
import * as PageList from '../custom_components/PageList';

const activeClassName: any = pagedSettingsClasses.Active

enum Renderer {
	Plugins,
	Themes,
	Updates,
	Unset
}

const RenderViewComponent = (componentType: Renderer): any => {
	Millennium.findElement(pluginSelf.settingsDoc, ".DialogContent_InnerWidth").then((element: NodeListOf<Element>) => { 

		switch (componentType) {
			case Renderer.Plugins:   
				ReactDOM.render(<PluginViewModal/>, element[0]);
				break;   	
			case Renderer.Themes:
				ReactDOM.render(<ThemeViewModal/>, element[0]);
				break;  
			case Renderer.Updates:
				ReactDOM.render(<UpdatesViewModal/>, element[0]);
				break;  
		}
	})
}

const PluginComponent: React.FC = () => {

	const [selected, setSelected] = useState<Renderer>();
	const nativeTabs = pluginSelf.settingsDoc.querySelectorAll(`.${Classes.PagedSettingsDialog_PageListItem}:not(.MillenniumTab)`)

	for (const tab of nativeTabs) {
		tab.addEventListener("click", () => {
			setSelected(Renderer.Unset);
		});
	}

	const componentUpdate = (type: Renderer) => {
		RenderViewComponent(type);
		setSelected(type)
		nativeTabs.forEach((element: HTMLElement) => {
			element.classList.remove(activeClassName);
		});
	}

	useEffect(() => {
		Millennium.findElement(pluginSelf.settingsDoc, ".DialogBody").then(_ => {
			if (pluginSelf?.OpenOnUpdatesPanel ?? false) {
				componentUpdate(Renderer.Updates)

				pluginSelf.OpenOnUpdatesPanel = false
			}
		})
	}, [])

	return (
		<>
		<PageList.Item
			bSelected={selected === Renderer.Plugins}
			icon=<CustomIcons.Plugins />
			title={locale.settingsPanelPlugins}
			onClick={() => {
				componentUpdate(Renderer.Plugins);
			}}
		/>
		<PageList.Item
			bSelected={selected === Renderer.Themes}
			icon=<CustomIcons.Themes />
			title={locale.settingsPanelThemes}
			onClick={() => {
				componentUpdate(Renderer.Themes);
			}}
		/>
		<PageList.Item
			bSelected={selected === Renderer.Updates}
			icon=<IconsModule.Update />
			title={locale.settingsPanelUpdates}
			onClick={() => {
				componentUpdate(Renderer.Updates);
			}}
		/>
		<PageList.Separator />
		</>
	);
}

/**
 * Hooks settings tabs components, and forces active overlayed panels to re-render
 * @todo A better, more integrated way of doing this, that doesn't involve runtime patching. 
 */
const hookSettingsComponent = () => {
	let processingItem = false;
	const elements = pluginSelf.settingsDoc.querySelectorAll(`.${Classes.PagedSettingsDialog_PageListItem}:not(.MillenniumTab)`);

	elements.forEach((element: HTMLElement, index: number) => {
		element.addEventListener('click', function(_: any) {

			if (processingItem) return

			console.log(pluginSelf.settingsDoc.querySelectorAll('.' + Classes.PageListSeparator))

			pluginSelf.settingsDoc.querySelectorAll('.' + Classes.PageListSeparator).forEach((element: HTMLElement) => element.classList.remove(Classes.Transparent))
			const click = new MouseEvent("click", { view: pluginSelf.settingsWnd, bubbles: true, cancelable: true })

			try {
				processingItem = true;
				if (index + 1 <= elements.length) elements[index + 1].dispatchEvent(click); else elements[index - 2].dispatchEvent(click);
				
				elements[index].dispatchEvent(click);
				processingItem = false;
			}
			catch (error) { console.log(error) }
		});
	})
}

function RenderSettingsModal(_context: any) 
{
	pluginSelf.settingsDoc = _context.m_popup.document
	pluginSelf.settingsWnd = _context.m_popup.window

	Millennium.findElement(_context.m_popup.document, "." + Classes.PagedSettingsDialog_PageList).then(element => {
		hookSettingsComponent()
		// Create a new div element
		var bufferDiv = document.createElement("div");
		bufferDiv.classList.add("millennium-tabs-list")

		element[0].prepend(bufferDiv);

		ReactDOM.render(<PluginComponent />, bufferDiv);
	})
}

export { RenderSettingsModal }