import { DialogBodyText, DialogButton, DialogHeader, IconsModule, SteamSpinner, TextField, callable } from '@steambrew/client';

import { useEffect, useState } from 'react';
import { fieldClasses, settingsClasses } from '../classes';
import Ansi from 'ansi-to-react';
import { ButtonsSection } from '../custom_components/ButtonsSection';
import { Separator } from '../components/ISteamComponents';

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

export const GetLogData = callable<[], LogData[]>('_get_plugin_logs');
const CopyText = callable<[{ data: string }], boolean>('_copy_to_clipboard');

const RenderLogViewer = ({ logs, setSelectedLog }: { logs: LogData; setSelectedLog: React.Dispatch<React.SetStateAction<LogData>> }) => {
	const [searchedLogs, setSearchedLogs] = useState<LogItem[]>(logs.logs);
	const [searchQuery, setSearchQuery] = useState<string>('');

	const [errorCount, setErrorCount] = useState<number>(0);
	const [warningCount, setWarningCount] = useState<number>(0);
	const [copyIcon, setCopyIcon] = useState<any>(<IconsModule.Copy />);

	const [logFontSize, setLogFontSize] = useState<number>(16);

	const FilterLogs = () => {
		// Count the number of errors and warnings
		setErrorCount(logs.logs.filter((log) => log.level === LogLevel.ERROR).length);
		setWarningCount(logs.logs.filter((log) => log.level === LogLevel.WARNING).length);
	};

	useEffect(() => {
		FilterLogs();
	}, [logs]);

	const CopyLogsToClipboard = () => {
		let logsToCopy = (searchQuery.length ? searchedLogs : logs.logs).map((log) => atob(log.message)).join('\n');

		if (CopyText({ data: logsToCopy })) {
			console.log('Copied logs to clipboard');
			setCopyIcon(<IconsModule.Checkmark />);
		} else {
			console.log('Failed to copy logs to clipboard');
		}

		setTimeout(() => {
			setCopyIcon(<IconsModule.Copy />);
		}, 2000);
	};

	const ShowMatchedLogsFromSearch = (e: React.ChangeEvent<HTMLInputElement>) => {
		setSearchQuery(e.target.value);

		let searchValue = e.target.value.toLowerCase();
		let matchedLogs = logs.logs.filter((log) => atob(log.message).toLowerCase().includes(searchValue));
		console.log(matchedLogs);

		setSearchedLogs(matchedLogs);
	};

	return (
		<>
			<div className="MillenniumLogs_Header">
				<div className="MillenniumLogs_HeaderNav">
					<DialogButton
						onClick={() => {
							setSelectedLog(undefined);
						}}
						className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}
					>
						<IconsModule.Carat />
						Back
					</DialogButton>
					<DialogHeader>{logs.name} output</DialogHeader>
				</div>

				<div className="MillenniumLogs_HeaderTextTypeContainer">
					<div className="MillenniumLogs_HeaderTextTypeCount" data-type="error" data-count={errorCount}>
						{errorCount} Errors{' '}
					</div>
					<div className="MillenniumLogs_HeaderTextTypeCount" data-type="warning" data-count={warningCount}>
						{warningCount} Warnings{' '}
					</div>
				</div>
			</div>

			<Separator />

			<div className="MillenniumLogs_TextContainer">
				<div className="MillenniumLogs_TextControls">
					<TextField placeholder="Search..." onChange={ShowMatchedLogsFromSearch} />

					<div className="MillenniumLogs_Icons">
						<DialogButton
							onClick={() => {
								setLogFontSize(logFontSize - 1);
							}}
							className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}
						>
							<IconsModule.Minus />
						</DialogButton>

						<DialogButton
							onClick={() => {
								setLogFontSize(logFontSize + 1);
							}}
							className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}
						>
							<IconsModule.Add />
						</DialogButton>

						<DialogButton
							onClick={() => {
								CopyLogsToClipboard();
							}}
							className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}
						>
							{copyIcon}
						</DialogButton>
					</div>
				</div>

				<pre className="MillenniumLogs_Text DialogInput DialogTextInputBase" style={{ fontSize: logFontSize + 'px' }}>
					{(searchQuery.length ? searchedLogs : logs.logs).map((log, index) => (
						<Ansi key={index}>{atob(log.message)}</Ansi>
					))}
				</pre>
			</div>
		</>
	);
};

interface RenderLogSelectorProps {
	logData: LogData[];
	setSelectedLog: React.Dispatch<React.SetStateAction<LogData>>;
}

const RenderLogSelector: React.FC<RenderLogSelectorProps> = ({ logData, setSelectedLog }) => {
	return logData === undefined ? (
		<SteamSpinner background={'transparent'} />
	) : (
		<>
			<DialogButton
				onClick={() => {
					SteamClient.Browser.OpenDevTools();
				}}
				className={settingsClasses.SettingsDialogButton}
			>
				Open Steam Dev Tools
			</DialogButton>

			{logData.map((log, index) => (
				<DialogButton
					key={index}
					onClick={() => {
						setSelectedLog(log ?? undefined);
					}}
					className={settingsClasses.SettingsDialogButton}
				>
					{log?.name}
				</DialogButton>
			))}
		</>
	);
};

export const LogsViewModal: React.FC = () => {
	const [logData, setLogData] = useState<LogData[]>(undefined);
	const [selectedLog, setSelectedLog] = useState<LogData>(undefined);

	useEffect(() => {
		GetLogData().then((data: any) => {
			setLogData(JSON.parse(data));
		});
	}, []);

	return selectedLog === undefined ? (
		<ButtonsSection>
			<RenderLogSelector logData={logData} setSelectedLog={setSelectedLog} />
		</ButtonsSection>
	) : (
		<RenderLogViewer logs={selectedLog} setSelectedLog={setSelectedLog} />
	);
};
