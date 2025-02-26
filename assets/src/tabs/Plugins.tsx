import React, { useEffect, useState } from 'react';
import { DialogBody, DialogBodyText, DialogButton, DialogFooter, DialogHeader, DialogLabel, Field, IconsModule, Millennium, Navigation, Toggle, callable, classMap, pluginSelf } from '@steambrew/client';
import { PluginComponent } from '../types';
import { locale } from '../locales';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { GetLogData, LogData, LogLevel } from './Logs';
import * as CustomIcons from '../custom_components/CustomIcons'

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
		&& typeof window.PLUGIN_LIST[plugin_name].renderPluginSettings === 'function' ? true : false
}

const EditPlugin: React.FC<EditPluginProps> = ({ plugin }) => {

	if (!isEditablePlugin(plugin?.data?.name)) {
		return <></>
	}

	return (
		<DialogButton className="_3epr8QYWw_FqFgMx38YEEm millenniumIconButton">
			<IconsModule.Settings height="16" />
		</DialogButton>
	)
}

interface PluginStatusProps {
	errors: number
	warnings: number
}

const PluginViewModal: React.FC = () => {

	const [plugins, setPlugins] = useState<PluginComponent[]>([])
	const [checkedItems, setCheckedItems] = useState<{ [key: number]: boolean }>({});
	const [pluginsWithLogs, setPluginsWithLogs] = useState<Map<string, PluginStatusProps>>()

	const FetchAllPlugins = () => {
		Millennium.callServerMethod("find_all_plugins").then((value: string) => {
			const json: PluginComponent[] = JSON.parse(value)

			setCheckedItems(
				json.map((plugin: PluginComponent, index: number) => ({ plugin, index }))
					.filter(({ plugin }) => plugin.enabled)
					.reduce((acc, { index }) => ({ ...acc, [index]: true }), {})
			)

			const pluginNames = json.map((plugin: PluginComponent) => plugin.data.common_name)
			let pluginsWithLogs: Map<string, PluginStatusProps> = new Map()

			GetLogData().then((result: any) => {
				const logData: LogData[] = JSON.parse(result)

				for (let plugin of logData) {
					// console.log("Plugin: ", plugin.name)
					if (pluginNames.includes(plugin.name)) {

						// count the amount of errors and warnings
						const errors = plugin.logs.filter((log) => log.level === LogLevel.ERROR).length
						const warnings = plugin.logs.filter((log) => log.level === LogLevel.WARNING).length

						pluginsWithLogs.set(plugin.name, { errors, warnings })
					}
				}

				setPluginsWithLogs(pluginsWithLogs)
				setPlugins(json)
			})
		})
			.then((result: any) => {
				pluginSelf.connectionFailed = false
				return result
			})
			.catch((_: any) => pluginSelf.connectionFailed = true)
	}

	useEffect(() => { FetchAllPlugins() }, [])

	const checkBoxChange = (index: number, checked: boolean): void => {
		console.log(checked)
		setCheckedItems({ ...checkedItems, [index]: checked });
	}

	const handleCheckboxChange = (index: number) => {

		/* Prevent users from disabling this plugin, as its vital */
		const updated: boolean = !checkedItems[index] || plugins[index]?.data?.name === "core"
		setCheckedItems({ ...checkedItems, [index]: updated });

		Millennium.callServerMethod("update_plugin_status", { plugin_name: plugins[index]?.data?.name, enabled: updated })
			.then((result: any) => {
				pluginSelf.connectionFailed = false
				return result
			})
	};

	const RenderStatusInfo = ({ pluginName }: { pluginName: string }) => {

		const pluginInfo = pluginsWithLogs?.get(pluginName)

		return (
			pluginInfo &&
			<>
				<p
					className='pluginStatus'
					style={{ fontSize: "12px", fontWeight: "500", display: "flex", alignContent: "center", gap: "5px" }}
				>
					Status:
					<p className='statusText' style={{ color: "#0ec50e", margin: "0" }}>Running
						{pluginInfo?.errors == 0 && pluginInfo?.warnings == 0 &&
							<span style={{ color: "#0ec50e" }}>, No issue found!</span>
						}
					</p>
					{pluginInfo?.errors > 0 &&
						<p className='statusText' style={{ color: "red", margin: "0" }}>with {pluginInfo?.errors} errors</p>
					}
					{pluginInfo?.warnings > 0 &&
						<p className='statusText' style={{ color: "rgb(255, 175, 0)", margin: "0" }}>and {pluginInfo?.warnings} warnings</p>
					}

					{(pluginInfo?.errors > 0 || pluginInfo?.warnings > 0) &&
						<span>(Check logs tab for more info)</span>
					}
				</p>
			</>

		)
	}

	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed />
	}

	const OpenPluginsFolder = async () => {
		const GetPluginsPath = callable<[], string>("get_plugins_dir")
		const pluginsPath = await GetPluginsPath()

		console.log("Opening plugins folder", pluginsPath)
		SteamClient.System.OpenLocalDirectoryInSystemExplorer(pluginsPath);
	}

	const GetMorePlugins = () => {
		SteamClient.System.OpenInSystemBrowser("https://steambrew.app/plugins");
	}

	return (
		<>
			<style>{`
				button.millenniumIconButton {
					padding: 9px 10px !important; 
					margin: 0 !important; 
					margin-right: 10px !important;
					display: flex;
					width: auto;
				}

				.millenniumPluginField {
					background: var(--main-editor-input-bg-color);
					border-radius: 4px !important;
					padding: 15px;
					margin-top: 10px;
				}
			`}</style>

			{/* <div className='pluginTabInfo' style={{
				marginTop: "5px",
				display: "flex",
				justifyContent: "space-between",
				alignItems: "center",
			}}>

				<p style={{ margin: "0px", fontSize: "13px" }}> Found {plugins.length} plugins, {Object.keys(checkedItems).filter((key: any) => checkedItems[key]).length} plugins enabled</p>

				<div style={{ display: "flex", gap: "10px" }}>
					<DialogButton onClick={FetchAllPlugins} style={{ padding: "0px 10px 0px" }}>
						<IconsModule.Refresh height="16" />
					</DialogButton>

					<DialogButton onClick={OpenPluginsFolder} style={{ padding: "0px 10px 0px" }}>
						<CustomIcons.Folder />
					</DialogButton>
				</div>
			</div> */}

			<Field
				label={`Found ${plugins.length} plugin(s), ${Object.keys(checkedItems).filter((key: any) => checkedItems[key]).length} plugin(s) enabled`}
				bottomSeparator='none'
				description={
					<>
						{locale.pluginPanelPluginTooltip}
						<a href="#" onClick={GetMorePlugins}>
							{locale.pluginPanelGetMorePlugins}
						</a>
					</>
				}
			>
				<div style={{ display: "flex", gap: "10px" }}>
					<DialogButton onClick={FetchAllPlugins} style={{ padding: "0px 10px 0px" }}>
						<IconsModule.Refresh height="16" />
					</DialogButton>

					<DialogButton onClick={OpenPluginsFolder} style={{ padding: "0px 10px 0px" }}>
						<CustomIcons.Folder />
					</DialogButton>
				</div>
			</Field>

			{plugins.map((plugin: PluginComponent, index: number) => (

				<div className='millenniumPluginField'>
					<Field
						key={index}
						label={plugin?.data?.common_name}
						description={plugin?.data?.description ?? locale.itemNoDescription}

						padding='standard'
						bottomSeparator='none'
					>
						<EditPlugin plugin={plugin} />
						<Toggle
							disabled={plugin?.data?.name == "core"}
							value={checkedItems[index]}
							onChange={(_checked: boolean) => handleCheckboxChange(index)}
						/>
					</Field>

					<RenderStatusInfo pluginName={plugin?.data?.common_name} />
				</div>
			))}
		</>
	)
}

export { PluginViewModal }