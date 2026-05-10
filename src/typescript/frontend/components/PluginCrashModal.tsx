import { ConfirmModal, DialogButton, Field, IconsModule, Navigation, pluginSelf, showModal } from '@steambrew/client';
import { backend } from '../utils/ffi';
import { settingsClasses } from '../utils/classes';
import { SettingsDialogSubHeader } from './SteamComponents';
import { OSType, PluginCrashInfo } from '../types';
import { setLogViewerAutoSelect } from '../settings/logs';
import { formatString, locale } from '../utils/localization-manager';

const WIN32_CODES: Record<number, string> = {
	0xc0000005: 'Access Violation',
	0xc0000094: 'Integer Divide by Zero',
	0xc0000096: 'Privileged Instruction',
	0xc00000fd: 'Stack Overflow',
	0xc000001d: 'Illegal Instruction',
	0xc0000409: 'Stack Buffer Overrun',
	0xe06d7363: 'C++ Exception',
	0xc0000135: 'DLL Not Found',
	0xc0000142: 'DLL Initialization Failed',
	0xc000007b: 'Bad Image Format',
	0xc0000017: 'Not Enough Memory',
	0xc0000025: 'Non-Continuable Exception',
	0x40000015: 'Fatal Application Exit',
	0x00000001: 'Error',
};

const UNIX_SIGNALS: Record<number, string> = {
	1: 'SIGHUP (Hangup)',
	2: 'SIGINT (Interrupt)',
	3: 'SIGQUIT (Quit)',
	4: 'SIGILL (Illegal instruction)',
	6: 'SIGABRT (Aborted)',
	7: 'SIGBUS (Bus error)',
	8: 'SIGFPE (Floating-point exception)',
	9: 'SIGKILL (Killed)',
	11: 'SIGSEGV (Segmentation fault)',
	13: 'SIGPIPE (Broken pipe)',
	14: 'SIGALRM (Alarm)',
	15: 'SIGTERM (Terminated)',
	31: 'SIGSYS (Bad system call)',
};

function formatExitCode(code: number): string {
	if (pluginSelf.platformType === OSType.Windows) {
		const u32 = code >>> 0;
		const hex = `0x${u32.toString(16).toUpperCase().padStart(8, '0')}`;
		const desc = WIN32_CODES[u32];
		return desc ? `${hex} (${desc})` : hex;
	}

	const desc = UNIX_SIGNALS[code];
	return desc ? `Signal ${code} — ${desc}` : `Signal ${code}`;
}

/** Tracks which plugins currently have an open crash modal, to prevent duplicate modals. */
const openModals = new Set<string>();

const CrashModal = ({ detail, closeModal, onResolve }: { detail: PluginCrashInfo; closeModal: () => void; onResolve: () => void }): React.ReactElement => {
	const pluginLabel = detail.displayName || detail.plugin;
	const disablePlugin = async () => {
		await backend.plugins.togglePlugin(JSON.stringify([{ plugin_name: detail.plugin, enabled: false }]));
		onResolve();
		closeModal();
	};

	const restartPlugin = async () => {
		await backend.plugins.togglePlugin(JSON.stringify([{ plugin_name: detail.plugin, enabled: false }]));
		await backend.plugins.togglePlugin(JSON.stringify([{ plugin_name: detail.plugin, enabled: true }]));
		onResolve();
		closeModal();
	};

	const openCrashFolder = () => {
		const target = detail.crashDumpDir || `${pluginSelf?.steamPath ?? ''}/millennium/crashes`;
		if (target) {
			SteamClient.System.OpenLocalDirectoryInSystemExplorer(target);
		}
	};

	const viewLogs = () => {
		setLogViewerAutoSelect(detail.plugin, detail.displayName ?? '');
		closeModal();
		Navigation.Navigate('/millennium/settings/logs');
	};

	const hasCrashDir = !!(detail.crashDumpDir || pluginSelf?.steamPath);

	return (
		<ConfirmModal
			bDisableBackgroundDismiss={true}
			strTitle={locale.crashModalTitle}
			strDescription={
				<>
					<Field
						description={
							<>
								{formatString(locale.crashModalDescPre, pluginLabel)}{' '}
								<a href={`https://github.com/SteamClientHomebrew/PluginDatabase/tree/main/plugins`} target="_blank" rel="noopener noreferrer">
									{locale.crashModalPluginDatabase}
								</a>{' '}
								{locale.crashModalDescPost}
							</>
						}
						bottomSeparator="none"
					/>

					<SettingsDialogSubHeader>{locale.crashModalDeveloperInfo}</SettingsDialogSubHeader>
					<Field label={locale.crashModalExitCode} description={locale.crashModalExitCodeDescription} icon={<IconsModule.ExclamationPoint color="red" />}>
						<code style={{ fontFamily: 'monospace', fontSize: '13px' }}>{formatExitCode(detail.exitCode)}</code>
					</Field>
					<Field label={locale.crashModalCrashDump} description={locale.crashModalCrashDumpDescription}>
						{hasCrashDir && (
							<DialogButton className={settingsClasses.SettingsDialogButton} onClick={openCrashFolder}>
								{locale.strOpenFolder}
							</DialogButton>
						)}
					</Field>
					<Field label={locale.crashModalPluginLogs} description={locale.crashModalPluginLogsDescription} bottomSeparator="none">
						<DialogButton className={settingsClasses.SettingsDialogButton} onClick={viewLogs}>
							{locale.strViewLogs}
						</DialogButton>
					</Field>
				</>
			}
			bAlertDialog={false}
			bHideCloseIcon={false}
			strOKButtonText={locale.crashModalRestartPlugin}
			strMiddleButtonText={locale.crashModalDisablePlugin}
			strCancelButtonText={locale.strDismiss}
			onOK={restartPlugin}
			onMiddleButton={disablePlugin}
			onCancel={closeModal}
		/>
	);
};

function openCrashModal(detail: PluginCrashInfo, onAfterResolve?: () => void) {
	openModals.add(detail.plugin);

	let modalWindow: ReturnType<typeof showModal>;
	const onResolve = () => {
		openModals.delete(detail.plugin);
		backend.crashHandler.acknowledgeCrash(detail.plugin);
		onAfterResolve?.();
	};
	const onClose = () => {
		openModals.delete(detail.plugin);
		modalWindow?.Close();
	};

	modalWindow = showModal(<CrashModal detail={detail} closeModal={onClose} onResolve={onResolve} />, pluginSelf.mainWindow, {
		popupHeight: 400,
		popupWidth: 580,
	});
}

/** Open the crash modal for a plugin (e.g. from the plugins settings page). */
export function showPluginCrashModal(detail: PluginCrashInfo, onAfterResolve?: () => void) {
	if (openModals.has(detail.plugin)) return;
	openCrashModal(detail, onAfterResolve);
}
