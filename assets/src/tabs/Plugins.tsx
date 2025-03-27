import React, { Fragment, useEffect, useRef, useState } from 'react';
import { ConfirmModal, DialogButton, Field, IconsModule, Millennium, Toggle, callable, pluginSelf, showModal } from '@steambrew/client';
import { PluginComponent } from '../types';
import { locale } from '../locales';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { GetLogData, LogData, LogLevel } from './Logs';
import * as CustomIcons from '../custom_components/CustomIcons';
import ReactDOM from 'react-dom';

interface EditPluginProps {
	plugin: PluginComponent;
}

declare global {
	interface Window {
		PLUGIN_LIST: any;
	}
}

const isEditablePlugin = (plugin_name: string) => {
	return window.PLUGIN_LIST && window.PLUGIN_LIST[plugin_name] && typeof window.PLUGIN_LIST[plugin_name].renderPluginSettings === 'function'
		? true
		: false;
};

const EditPlugin: React.FC<EditPluginProps> = ({ plugin }) => {
	if (!isEditablePlugin(plugin?.data?.name)) {
		return <></>;
	}

	return (
		<DialogButton className="_3epr8QYWw_FqFgMx38YEEm millenniumIconButton">
			<IconsModule.Settings height="16" />
		</DialogButton>
	);
};

interface PluginStatusProps {
	errors: number;
	warnings: number;
}

interface UpdatedPluginProps {
	plugin_name: string;
	enabled: boolean;
}

const FindAllPlugins = callable<[], string>('find_all_plugins');
const UpdatePluginStatus = callable<[{ pluginJson: string }], any>('update_plugin_status');
const GetPluginsPath = callable<[], string>('get_plugins_dir');

const PluginViewModal: React.FC = () => {
	const [plugins, setPlugins] = useState<PluginComponent[]>([]);
	const [checkedItems, setCheckedItems] = useState<{ [key: number]: boolean }>({});
	const [pluginsWithLogs, setPluginsWithLogs] = useState<Map<string, PluginStatusProps>>();

	const FetchAllPlugins = () => {
		FindAllPlugins()
			.then((value: string) => {
				const json: PluginComponent[] = JSON.parse(value);

				setCheckedItems(
					json
						.map((plugin: PluginComponent, index: number) => ({ plugin, index }))
						.filter(({ plugin }) => plugin.enabled)
						.reduce((acc, { index }) => ({ ...acc, [index]: true }), {}),
				);

				const pluginNames = json.map((plugin: PluginComponent) => plugin.data.common_name);
				let pluginsWithLogs: Map<string, PluginStatusProps> = new Map();

				GetLogData().then((result: any) => {
					const logData: LogData[] = JSON.parse(result);

					for (let plugin of logData) {
						if (pluginNames.includes(plugin.name)) {
							const errors = plugin.logs.filter((log) => log.level === LogLevel.ERROR).length;
							const warnings = plugin.logs.filter((log) => log.level === LogLevel.WARNING).length;
							pluginsWithLogs.set(plugin.name, { errors, warnings });
						}
					}

					setPluginsWithLogs(pluginsWithLogs);
					setPlugins(json);
				});
			})
			.then((result: any) => {
				pluginSelf.connectionFailed = false;
				return result;
			})
			.catch((_: any) => (pluginSelf.connectionFailed = true));
	};

	useEffect(() => {
		FetchAllPlugins();
	}, []);

	const [updatedPlugins, setUpdatedPlugins] = useState<UpdatedPluginProps[]>([]);

	const handleCheckboxChange = (index: number) => {
		/* Prevent users from disabling this plugin, as its vital */
		const updated: boolean = !checkedItems[index] || plugins[index]?.data?.name === 'core';

		setCheckedItems({ ...checkedItems, [index]: updated });
		setUpdatedPlugins([...updatedPlugins, { plugin_name: plugins[index]?.data?.name, enabled: updated }]);
	};

	const SavePluginChanges = () => {
		const onOK = () => {
			UpdatePluginStatus({ pluginJson: JSON.stringify(updatedPlugins) })
				.then(() => {
					pluginSelf.connectionFailed = false;
				})
				.catch((_: any) => {
					pluginSelf.connectionFailed = true;
				});
		};

		showModal(
			<ConfirmModal
				strTitle={locale.optionReloadRequired}
				strDescription={locale.optionPluginNeedsReload}
				strOKButtonText={locale.optionReloadNow}
				onOK={onOK}
			/>,
			pluginSelf.windows['Millennium'],
			{
				bNeverPopOut: false,
			},
		);
	};

	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed />;
	}

	const OpenPluginsFolder = async () => {
		const pluginsPath = await GetPluginsPath();

		console.log('Opening plugins folder', pluginsPath);
		SteamClient.System.OpenLocalDirectoryInSystemExplorer(pluginsPath);
	};

	const GetMorePlugins = () => {
		SteamClient.System.OpenInSystemBrowser('https://steambrew.app/plugins');
	};

	const RenderStatusTooltip = ({ plugin, index }: { plugin: PluginComponent; index: number }) => {
		const [position, setPosition] = useState({ top: 0, left: '0px' });

		useEffect(() => {
			const element: HTMLElement = pluginSelf.windows['Millennium'].document.querySelector(`#pluginStatusDot-${index}`);
			if (!element) {
				console.error('Element not found');
				return;
			}

			const rect = element.getBoundingClientRect();
			setPosition({
				top: rect.top + rect.height + 10,
				left: rect.right - rect.width + 'px',
			});
		}, []);

		const errors = pluginsWithLogs?.get(plugin?.data?.common_name)?.errors;
		const warnings = pluginsWithLogs?.get(plugin?.data?.common_name)?.warnings;

		const StatusToString = () => {
			if (errors > 0) {
				return `Found ${errors} error(s) in ${plugin.data.common_name}! Its possible these errors are not critical, but you should check the logs tab for more info.`;
			}
			if (warnings > 0) {
				return `Found ${warnings} warning(s) in ${plugin.data.common_name}! Its possible these warnings are not critical, but you should check the logs tab for more info.`;
			} else {
				return plugin.data.common_name + ' is running without any issues!';
			}
		};

		return ReactDOM.createPortal(
			<div className="_3vg1vYU7iTWqONciv9cuJN _1Ye_0niF2UqB8uQTbm8B6B" style={{ top: position.top, left: position.left }}>
				<div className="_2FxbHJzYoH024ko7zqcJOf" style={{ maxWidth: '50vw' }}>
					{StatusToString()}
				</div>
			</div>,
			pluginSelf.windows['Millennium'].document.body,
		);
	};

	const RenderPlugin = ({ plugin, index }: { plugin: PluginComponent; index: number }) => {
		enum Level {
			Error = 'red',
			Warning = 'rgb(255, 175, 0)',
			OK = '#0ec50e',
		}

		const pluginLogs = pluginsWithLogs?.get(plugin?.data?.common_name);
		const backgroundColor = pluginLogs?.errors > 0 ? Level.Error : pluginLogs?.warnings > 0 ? Level.Warning : Level.OK;
		const [isHovering, setIsHovering] = useState(false);

		return (
			<div className="millenniumPluginField">
				<Field
					key={index}
					label={
						<div className="pluginNameContainer">
							{pluginLogs && checkedItems[index] && (
								<div
									className="pluginStatusDot"
									style={{ backgroundColor: backgroundColor }}
									id={`pluginStatusDot-${index}`}
									onMouseEnter={() => setIsHovering(true)}
									onMouseLeave={() => setIsHovering(false)}
								/>
							)}
							{plugin?.data?.common_name}
							{plugin?.data?.version && <div style={{ fontSize: '12px', color: 'grey' }}>{plugin?.data?.version}</div>}
						</div>
					}
					description={plugin?.data?.description ?? locale.itemNoDescription}
					padding="standard"
					bottomSeparator="none"
				>
					<EditPlugin plugin={plugin} />
					<Toggle
						disabled={plugin?.data?.name == 'core'}
						value={checkedItems[index]}
						onChange={(_checked: boolean) => handleCheckboxChange(index)}
					/>
				</Field>

				{isHovering && <RenderStatusTooltip plugin={plugin} index={index} />}
			</div>
		);
	};

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
					padding: 15px 25px 15px 25px;
					margin-top: 10px;
				}

				.pluginNameContainer {
					display: flex;
					align-items: center;
					gap: 11px;
				}

				.pluginStatusDot {
					width: 8px;
					height: 8px;
					border-radius: 50% !important;
				}
					
				button.DialogButton._DialogLayout.Secondary.Focusable {
					width: fit-content;
				}
			`}</style>

			<Field
				label={`Steam Client Plugins`}
				bottomSeparator="none"
				description={
					<>
						{locale.pluginPanelPluginTooltip}
						<a href="#" onClick={GetMorePlugins}>
							{locale.pluginPanelGetMorePlugins}
						</a>
					</>
				}
			>
				<div style={{ display: 'flex', gap: '10px' }}>
					{updatedPlugins.length > 0 && (
						<DialogButton onClick={SavePluginChanges} style={{ padding: '0px 10px 0px' }}>
							{locale.optionSaveChanges}
						</DialogButton>
					)}
					<DialogButton onClick={FetchAllPlugins} style={{ padding: '0px 10px 0px' }}>
						<IconsModule.Refresh height="16" />
					</DialogButton>

					<DialogButton onClick={OpenPluginsFolder} style={{ padding: '0px 10px 0px' }}>
						<CustomIcons.Folder />
					</DialogButton>
				</div>
			</Field>

			{plugins.map((plugin: PluginComponent, index: number) => (
				<RenderPlugin key={index} plugin={plugin} index={index} />
			))}
		</>
	);
};

export { PluginViewModal };
