import { DialogButton, DialogControlsSection, IconsModule, TextField, callable, pluginSelf } from '@steambrew/client';
import { settingsClasses } from '../../utils/classes';
import Ansi from 'ansi-to-react';
import React, { Component } from 'react';
import { locale } from '../../../locales';
import { PyGetLogData, PySetClipboardText } from '../../utils/ffi';
import { DesktopTooltip, SettingsDialogSubHeader } from '../../components/SteamComponents';
import { FaCircleCheck } from 'react-icons/fa6';

export type LogItem = {
	level: LogLevel;
	message: string;
};

export interface LogData {
	name: string;
	logs: LogItem[];
}

export enum LogLevel {
	INFO,
	WARNING,
	ERROR,
}

interface RenderLogViewerState {
	logData: LogData[] | undefined;
	selectedLog: LogData | undefined;
	searchedLogs: LogItem[];
	searchQuery: string;
	errorCount: number;
	warningCount: number;
	copyIcon: any;
	logFontSize: number;
}

export class RenderLogViewer extends Component<{}, RenderLogViewerState> {
	constructor(props: {}) {
		super(props);
		this.state = {
			logData: undefined,
			selectedLog: undefined,
			searchedLogs: [],
			searchQuery: '',
			errorCount: 0,
			warningCount: 0,
			copyIcon: <IconsModule.Copy />,
			logFontSize: 16,
		};
	}

	componentDidMount() {
		PyGetLogData().then((data: any) => {
			const parsed = JSON.parse(data);
			this.setState({ logData: parsed });
		});
	}

	componentDidUpdate(_prevProps: {}, prevState: RenderLogViewerState) {
		/** Kinda a hacky way to change the title, but its good enough and doesn't interfere with anything */
		if (this.state.selectedLog !== prevState.selectedLog) {
			const container = pluginSelf.mainWindow.document.querySelector('.DialogHeader');
			if (container) {
				container.textContent = this.state.selectedLog?.name || locale.settingsPanelLogs;
			}
			this.aggregateLogStats();
		}
	}

	aggregateLogStats() {
		if (!this.state.selectedLog) return;

		const errorCount = this.state.selectedLog.logs.filter((log) => log.level === LogLevel.ERROR).length;
		const warningCount = this.state.selectedLog.logs.filter((log) => log.level === LogLevel.WARNING).length;

		this.setState({ errorCount, warningCount });
	}

	exportToClipBoard = () => {
		const { selectedLog, searchQuery, searchedLogs } = this.state;
		if (!selectedLog) return;

		const logsToCopy = (searchQuery.length ? searchedLogs : selectedLog.logs)
			.map((log) => atob(log.message))
			.join('')
			/** Strip all ANSI colors that were provided by Millennium */
			.replace(/\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])/g, '');

		if (PySetClipboardText({ data: logsToCopy })) {
			this.setState({ copyIcon: <IconsModule.Checkmark /> });

			setTimeout(() => {
				this.setState({ copyIcon: <IconsModule.Copy /> });
			}, 2000);
		}
	};

	filterLogsBySearchQuery = (e: React.ChangeEvent<HTMLInputElement>) => {
		if (!this.state.selectedLog) return;

		const searchQuery = e.target.value;
		const matchedLogs = this.state.selectedLog.logs.filter((log) => atob(log.message).toLowerCase().includes(searchQuery.toLowerCase()));

		this.setState({ searchQuery, searchedLogs: matchedLogs });
	};

	renderLogItemButton = (log: LogData) => {
		const hasError = log.logs.some((log) => log.level === LogLevel.ERROR);
		const hasWarning = log.logs.some((log) => log.level === LogLevel.WARNING);

		let ErrorTooltip = null;

		if (hasError || hasWarning) {
			const isError = hasError;
			const messageType = isError ? 'encountered errors' : 'issued warnings';
			const color = isError ? 'red' : '#ffc82c';

			ErrorTooltip = (
				<DesktopTooltip toolTipContent={`${log.name} ${messageType}.`} direction="top" style={{ height: '16px', width: '16px' }}>
					<IconsModule.ExclamationPoint color={color} style={{ height: '16px', width: '16px' }} />
				</DesktopTooltip>
			);
		} else {
			ErrorTooltip = (
				<DesktopTooltip toolTipContent={`${log.name} has no errors or warnings.`} direction="top" style={{ height: '16px', width: '16px' }}>
					<FaCircleCheck color="green" style={{ height: '16px', width: '16px' }} />
				</DesktopTooltip>
			);
		}

		return (
			<DialogButton
				key={log.name}
				onClick={() => this.setState({ selectedLog: log, searchedLogs: log.logs })}
				className={settingsClasses.SettingsDialogButton}
				style={{ display: 'flex', gap: '10px' }}
			>
				{ErrorTooltip}
				{log?.name}
			</DialogButton>
		);
	};

	renderSelector() {
		const { logData } = this.state;
		if (!logData) return null;

		/** Millennium specific logs */
		const millenniumNames = new Set(['Millennium', 'Standard Output', 'Package Manager']);

		const { millenniumItems, userPlugins } = logData.reduce(
			(acc, item) => {
				(millenniumNames.has(item.name) ? acc.millenniumItems : acc.userPlugins).push(item);
				return acc;
			},
			{ millenniumItems: [], userPlugins: [] },
		);

		return [
			<SettingsDialogSubHeader>Millennium Logs</SettingsDialogSubHeader>,
			millenniumItems.map((log) => this.renderLogItemButton(log)),
			<SettingsDialogSubHeader style={{ marginTop: '20px' }}>User Plugins</SettingsDialogSubHeader>,
			userPlugins.map((log) => this.renderLogItemButton(log)),
		];
	}

	renderViewer() {
		const { selectedLog, searchedLogs, searchQuery, errorCount, warningCount, copyIcon, logFontSize } = this.state;

		return (
			<div className="MillenniumLogs_TextContainer">
				<div className="MillenniumLogs_ControlSection">
					<div className="MillenniumLogs_NavContainer">
						<DialogButton onClick={() => this.setState({ selectedLog: null })} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
							<IconsModule.Carat direction="left" />
							Back
						</DialogButton>
						{/* @ts-ignore */}
						<TextField placeholder="Search..." onChange={this.filterLogsBySearchQuery} />
					</div>

					<div className="MillenniumLogs_TextControls">
						<div className="MillenniumLogs_HeaderTextTypeContainer">
							<div className="MillenniumLogs_HeaderTextTypeCount" data-type="error" data-count={errorCount}>
								{errorCount} Errors
							</div>
							<div className="MillenniumLogs_HeaderTextTypeCount" data-type="warning" data-count={warningCount}>
								{warningCount} Warnings
							</div>
						</div>

						<div className="MillenniumLogs_Icons">
							<DialogButton
								onClick={() => this.setState({ logFontSize: logFontSize - 1 })}
								className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}
							>
								<IconsModule.Minus />
							</DialogButton>
							<DialogButton
								onClick={() => this.setState({ logFontSize: logFontSize + 1 })}
								className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}
							>
								<IconsModule.Add />
							</DialogButton>
							<DialogButton onClick={this.exportToClipBoard} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
								{copyIcon}
							</DialogButton>
						</div>
					</div>
				</div>

				<pre className="MillenniumLogs_Text DialogInput DialogTextInputBase" style={{ fontSize: logFontSize + 'px' }}>
					{(searchQuery.length ? searchedLogs : selectedLog.logs).map((log, index) => (
						<Ansi key={index}>{atob(log.message)}</Ansi>
					))}
				</pre>
			</div>
		);
	}

	render() {
		const { selectedLog } = this.state;
		return <DialogControlsSection className="MillenniumButtonsSection">{!selectedLog ? this.renderSelector() : this.renderViewer()}</DialogControlsSection>;
	}
}
