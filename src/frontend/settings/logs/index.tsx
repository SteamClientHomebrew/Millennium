/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { DialogButton, DialogControlsSection, IconsModule, TextField, joinClassNames, pluginSelf } from '@steambrew/client';
import { settingsClasses } from '../../utils/classes';
import Ansi from 'ansi-to-react';
import React, { Component } from 'react';
import { formatString, locale } from '../../utils/localization-manager';
import { PyGetLogData } from '../../utils/ffi';
import { DesktopTooltip, SettingsDialogSubHeader } from '../../components/SteamComponents';
import { IconButton } from '../../components/IconButton';

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
	displayCopiedText: boolean;
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
			displayCopiedText: false,
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
			const container = pluginSelf.mainWindow.document.querySelector('.MillenniumSettings .DialogHeader');
			if (container) {
				container.textContent = this.state.selectedLog?.name || locale.settingsPanelLogs;
			}
			this.aggregateLogStats();
		}
	}

	aggregateLogStats() {
		if (!this.state.selectedLog) return;

		const errorCount = (this.state.selectedLog?.logs?.filter((log) => log.level === LogLevel.ERROR) ?? []).length;
		const warningCount = (this.state.selectedLog?.logs?.filter((log) => log.level === LogLevel.WARNING) ?? []).length;

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

		const textarea = document.createElement('textarea');
		textarea.value = logsToCopy;
		document.body.appendChild(textarea);
		textarea.select();
		document.execCommand('copy');
		document.body.removeChild(textarea);

		this.setState({ displayCopiedText: true });

		setTimeout(() => {
			this.setState({ displayCopiedText: false });
		}, 2000);
	};

	filterLogsBySearchQuery = (e: React.ChangeEvent<HTMLInputElement>) => {
		if (!this.state.selectedLog) return;

		const searchQuery = e.target.value;
		const matchedLogs = this.state.selectedLog.logs.filter((log) => atob(log.message).toLowerCase().includes(searchQuery.toLowerCase()));

		this.setState({ searchQuery, searchedLogs: matchedLogs });
	};

	renderLogItemButton = (log: LogData) => {
		const errors = log?.logs?.filter((log) => log.level === LogLevel.ERROR) ?? [];
		const warnings = log?.logs?.filter((log) => log.level === LogLevel.WARNING) ?? [];

		const messageType = errors?.length !== 0 ? locale.statusEncounteredErrors : locale.statusIssuedWarnings;

		return (
			<DialogButton
				key={log.name}
				onClick={() => this.setState({ selectedLog: log, searchedLogs: log.logs })}
				className={joinClassNames('MillenniumButton', 'MillenniumLogs_LogItemButton', settingsClasses.SettingsDialogButton)}
				data-error-count={errors?.length}
				data-warning-count={warnings?.length}
			>
				<DesktopTooltip bDisabled={errors?.length === 0 && warnings?.length === 0} toolTipContent={`${log.name} ${messageType}.`} direction="top">
					<IconsModule.ExclamationPoint />
				</DesktopTooltip>
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

		let components = [];

		if (millenniumItems.length) {
			components.push(
				<DialogControlsSection>
					<SettingsDialogSubHeader>{locale.logsMillenniumHeader}</SettingsDialogSubHeader>
					<div className="MillenniumButtonsSection MillenniumLogsSection">{millenniumItems.map((log) => this.renderLogItemButton(log))}</div>
				</DialogControlsSection>,
			);
		}

		if (userPlugins.length) {
			components.push(
				<DialogControlsSection>
					<SettingsDialogSubHeader>{locale.logsUserPluginsHeader}</SettingsDialogSubHeader>
					<div className="MillenniumButtonsSection MillenniumLogsSection">{userPlugins.map((log) => this.renderLogItemButton(log))}</div>
				</DialogControlsSection>,
			);
		}

		return components;
	}

	renderViewer() {
		const { selectedLog, searchedLogs, searchQuery, errorCount, warningCount, displayCopiedText, logFontSize } = this.state;

		return (
			<div className="MillenniumLogs_TextContainer">
				<div className="MillenniumLogs_ControlSection">
					<div className="MillenniumLogs_NavContainer">
						<DialogButton onClick={() => this.setState({ selectedLog: null })} className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`}>
							<IconsModule.Carat direction="left" />
							{locale.buttonBack}
						</DialogButton>
						{/* @ts-ignore */}
						<TextField placeholder={locale.placeholderSearch} onChange={this.filterLogsBySearchQuery} />
					</div>

					<div className="MillenniumLogs_TextControls">
						<div className="MillenniumLogs_HeaderTextTypeContainer">
							<div className="MillenniumLogs_HeaderTextTypeCount" data-type="error" data-count={errorCount}>
								{formatString(locale.logsErrorCount, String(errorCount))}
							</div>
							<div className="MillenniumLogs_HeaderTextTypeCount" data-type="warning" data-count={warningCount}>
								{formatString(locale.logsWarningCount, String(warningCount))}
							</div>
						</div>

						<div className="MillenniumLogs_Icons">
							<IconButton name="Minus" onClick={() => this.setState({ logFontSize: logFontSize - 1 })} text={locale.buttonDecreaseFontSize} />
							<IconButton name="Add" onClick={() => this.setState({ logFontSize: logFontSize + 1 })} text={locale.buttonIncreaseFontSize} />
							<IconButton
								name={displayCopiedText ? 'Checkmark' : 'Copy'}
								onClick={this.exportToClipBoard}
								text={locale.buttonCopyToClipboard}
							/>
						</div>
					</div>
				</div>

				<pre className="MillenniumLogs_Text DialogInput DialogTextInputBase" style={{ fontSize: logFontSize + 'px' }}>
					{(searchQuery?.length ? searchedLogs : selectedLog?.logs)?.map((log, index) => (
						<Ansi key={index}>{atob(log?.message)}</Ansi>
					))}
				</pre>
			</div>
		);
	}

	render() {
		const { selectedLog } = this.state;
		return !selectedLog ? this.renderSelector() : this.renderViewer();
	}
}
