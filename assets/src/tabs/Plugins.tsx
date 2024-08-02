import React, { useEffect, useState } from 'react';
import { DialogBody, DialogButton, DialogHeader, IconsModule, Millennium, Toggle, classMap, pluginSelf } from '@millennium/ui';
import { PluginComponent } from '../types';
import { locale } from '../locales';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { Field } from '../custom_components/Field';

interface EditPluginProps {
	plugin: PluginComponent
}

const isEditablePlugin = (plugin_name: string) => {
	return window.PLUGIN_LIST && window.PLUGIN_LIST[plugin_name] 
	&& typeof window.PLUGIN_LIST[plugin_name].renderPluginSettings === 'function' ? true : false
}

const EditPlugin: React.FC<EditPluginProps> = ({ plugin }) => {

	if (!isEditablePlugin(plugin?.data?.name)) {
		return <></>
	}

	return (
		<DialogButton className="_3epr8QYWw_FqFgMx38YEEm" style={{marginTop: 0, marginLeft: 0, marginRight: 15}}>
			<IconsModule.Settings />
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
		<DialogHeader>{locale.settingsPanelPlugins}</DialogHeader>
		<DialogBody className={classMap.SettingsDialogBodyFade}>
			{plugins.map((plugin: PluginComponent, index: number) => (
				<Field
					key={index}
					label={plugin?.data?.common_name}
					description={plugin?.data?.description ?? locale.itemNoDescription}
				>
					<EditPlugin plugin={plugin}/>
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