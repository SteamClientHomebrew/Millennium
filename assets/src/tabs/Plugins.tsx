import React, { useEffect, useState } from 'react';
import {
	DialogBody,
	DialogButton,
	DialogHeader,
	IconsModule,
	Millennium,
	Toggle,
	classMap,
	pluginSelf,
	showModal,
	MillenniumModuleSettings,
	MillenniumSettingTabs,
} from '@steambrew/client';
import { PluginComponent } from '../types';
import { locale } from '../locales';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { Field } from '../custom_components/Field';
import RenderPluginSettings from '../custom_components/PluginSettings';
import { CreatePopup } from '../components/CreatePopup';

// class TestPopup extends React.Component<any> {
// 	render() {
// 		return (
// 			<ModalPosition>
// 				<SidebarNavigation pages={[{title: 'test', content: <h2>Test</h2>}]}>
// 				</SidebarNavigation>
// 			</ModalPosition>
// 		)
// 	}
// }

const PluginSettings = (plugin: PluginComponent)=> {
	//TODO: find out why CreatePopup fails when rendering a SidebarNavigation
	// Seems to be something with the "Router" context being empty so it can't get location?
	// CreatePopup is better than showModal because it can be resized and text fields are interactable but it doesn't want to render SidebarNavigation

	// const popup = new CreatePopup(TestPopup, "testing", {
	// 	title: 'Testing window',
	// 	popup_class: "fullheight",
	// 	body_class: "fullheight ModalDialogBody DesktopUI ",
	// 	html_class: "client_chat_frame fullheight ModalDialogPopup ",
	// 	eCreationFlags: 18,
	// 	dimensions: {width: 850, height: 722},
	// 	replace_existing_popup: false,
	// }, {plugin: plugin})
	// popup.Show();

	showModal(<RenderPluginSettings plugin={plugin} />, pluginSelf.settingsWnd, {
		strTitle: `Editing ${plugin.data.common_name} settings`,
		popupHeight: 675,
		popupWidth: 850,
	})
}

interface EditPluginProps {
	plugin: PluginComponent
}

declare global {
	interface Window {
		PLUGIN_LIST: any
	}
}

const isEditablePlugin = (plugin_name: string) => {
	return window.PLUGIN_LIST && window.PLUGIN_LIST[plugin_name] 
	&& window.PLUGIN_LIST[plugin_name].settings
	&& (window.PLUGIN_LIST[plugin_name].settings instanceof MillenniumModuleSettings
		|| window.PLUGIN_LIST[plugin_name].settings instanceof MillenniumSettingTabs)
}

const RenderEditPlugin: React.FC<EditPluginProps> = ({ plugin }) => {

	if (!isEditablePlugin(plugin?.data?.name)) {
		return <></>
	}

	return (
		<DialogButton
			onClick={() => PluginSettings(plugin)}
			className="_3epr8QYWw_FqFgMx38YEEm millenniumIconButton"
		>
			<IconsModule.Settings height="16" />
		</DialogButton>
	)
}

const PluginViewModal: React.FC = () => {

	const [plugins, setPlugins] = useState<PluginComponent[]>([])
	const [checkedItems, setCheckedItems] = useState<{ [key: number]: boolean }>({});

	useEffect(() => {
		Millennium.callServerMethod("find_all_plugins").then((value: string) => {
			const json: PluginComponent[] = JSON.parse(value)

			setCheckedItems(
				json.map((plugin: PluginComponent, index: number) => ({ plugin, index })).filter(({ plugin }) => plugin.enabled)
				.reduce((acc, { index }) => ({ ...acc, [index]: true }), {})
			)
			setPlugins(json)
		})
		.then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        })
		.catch((_: any) => pluginSelf.connectionFailed = true)
	}, [])

	const checkBoxChange = (index: number, checked: boolean): void => {
		console.log(checked)
		setCheckedItems({ ...checkedItems, [index]: checked});
	}

	const handleCheckboxChange = (index: number) => {
		
		/* Prevent users from disabling this plugin, as its vital */
		const updated: boolean = !checkedItems[index] || plugins[index]?.data?.name === "core"
		setCheckedItems({ ...checkedItems, [index]: updated});

		Millennium.callServerMethod("update_plugin_status", { plugin_name: plugins[index]?.data?.name, enabled: updated })
		.then((result: any) => {
            pluginSelf.connectionFailed = false
            return result
        })
	};


	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed/>
	}

	return (
		<>
		<style>
		{`
			button.millenniumIconButton {
				padding: 9px 10px !important; 
				margin: 0 !important; 
				margin-right: 10px !important;
				display: flex;
				width: auto;
			}
		`}
		</style>

		<DialogHeader>{locale.settingsPanelPlugins}</DialogHeader>
		<DialogBody className={classMap.SettingsDialogBodyFade}>
			{plugins.map((plugin: PluginComponent, index: number) => (
				<Field
					key={index}
					label={plugin?.data?.common_name}
					description={plugin?.data?.description ?? locale.itemNoDescription}
				>
					<RenderEditPlugin plugin={plugin}/>
					<Toggle 
						disabled={plugin?.data?.name == "core"} 
						value={checkedItems[index]} 
						onChange={(_checked: boolean) => handleCheckboxChange(index)}
					/>
				</Field>
			))} 
		</DialogBody>
		</>
	)
}

export { PluginViewModal }
