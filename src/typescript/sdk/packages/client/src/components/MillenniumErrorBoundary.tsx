import { FunctionComponent, useEffect, useReducer, useState } from 'react';
import { getLikelyErrorSourceFromValveReactError, ValveReactErrorInfo } from '../hooks/error-from-source';

interface MillenniumErrorBoundaryProps {
	error: ValveReactErrorInfo;
	errorKey: string;
	identifier: string;
	reset: () => void;
}

declare global {
	interface Window {
		SystemNetworkStore?: any;
	}
}

interface MillenniumUpdates {
	hasUpdate: boolean;
	updateInProgress: boolean;
	newVersion: { tag_name: string } | null;
	platformRelease: { browser_download_url: string; size: number } | null;
}

// 0 = Windows, 1 = Linux, 2 = Darwin
const enum OSType {
	Windows = 0,
	Linux = 1,
	Darwin = 2,
}

interface StartConfig {
	millenniumUpdates?: MillenniumUpdates;
	platformType: OSType;
	millenniumLinuxUpdateScript?: string;
}

// The stub `ffi` from millennium-api is baked into the loader bundle and always resolves to undefined.
// window.MILLENNIUM_API.ffi is the real implementation (pluginName, route) injected at runtime by the loader.
// The Millennium internal frontend uses plugin name 'core' for all Core_* FFI calls.
function coreFFI<TArgs extends any[], TReturn>(route: string): (...args: TArgs) => Promise<TReturn> {
	return (...args: TArgs): Promise<TReturn> => {
		const realFfi = (window as any).MILLENNIUM_API?.ffi;
		if (typeof realFfi !== 'function') return Promise.resolve(undefined as any);
		return realFfi('core', route)(...args);
	};
}

const getStartConfig = coreFFI<[], StartConfig>('Core_GetStartConfig');
const checkMillenniumUpdate = coreFFI<[channel: string], MillenniumUpdates>('Core_CheckMillenniumUpdate');
const setPluginStatus = coreFFI<[pluginJson: string], void>('Core_ChangePluginStatus');
const doUpdateMillennium = coreFFI<[downloadUrl: string, downloadSize: number, background: boolean], void>('Core_UpdateMillennium');

type UpdateChannel = 'stable' | 'beta';
type CheckState = 'idle' | 'checking' | 'found' | 'up-to-date' | 'error';

const MillenniumErrorBoundary: FunctionComponent<MillenniumErrorBoundaryProps> = ({ error, identifier, reset }) => {
	const [actionLog, addLogLine] = useReducer((log: string, line: string) => log + '\n' + line, '');
	const [startConfig, setStartConfig] = useState<StartConfig | null>(null);
	const [channel, setChannel] = useState<UpdateChannel>('stable');
	const [checkState, setCheckState] = useState<CheckState>('idle');
	const [errorSource, wasCausedByPlugin, shouldReportToValve] = getLikelyErrorSourceFromValveReactError(error);

	useEffect(() => {
		if (!shouldReportToValve) window.__ERRORBOUNDARY_HOOK_INSTANCE.temporarilyDisableReporting();
		getStartConfig().then(setStartConfig);
	}, []);

	const restartSteam = () => {
		addLogLine('Restarting Steam...');
		SteamClient.User.StartRestart(false);
	};

	const disablePlugin = async () => {
		addLogLine(`Disabling ${errorSource}...`);
		await setPluginStatus(JSON.stringify([{ plugin_name: errorSource, enabled: false }]));
	};

	const onCheckForUpdates = async () => {
		setCheckState('checking');
		try {
			const updates = await checkMillenniumUpdate(channel);
			setStartConfig((prev) => (prev ? { ...prev, millenniumUpdates: updates } : prev));
			setCheckState(updates?.hasUpdate ? 'found' : 'up-to-date');
		} catch {
			setCheckState('error');
		}
	};

	const onUpdateMillennium = async () => {
		const url = startConfig?.millenniumUpdates?.platformRelease?.browser_download_url;
		const size = startConfig?.millenniumUpdates?.platformRelease?.size ?? 0;
		if (!url) return;
		addLogLine('Starting Millennium update...');
		await doUpdateMillennium(url, size, false);
		restartSteam();
	};

	const update = startConfig?.millenniumUpdates;
	const isWindows = startConfig?.platformType === OSType.Windows;
	const isLinuxOrMac = startConfig?.platformType === OSType.Linux || startConfig?.platformType === OSType.Darwin;

	const btn = { marginRight: '5px', padding: '5px' };
	const sel = { height: '28px', marginRight: '5px', padding: '0 4px', verticalAlign: 'middle' };

	return (
		<>
			<style>{`*:has(> .MillenniumErrorBoundary) { overflow: scroll !important; }`}</style>
			<div
				style={{ overflow: 'auto', marginLeft: '15px', color: 'white', fontSize: '16px', userSelect: 'auto', backgroundColor: 'black', marginTop: '48px' }}
				className="MillenniumErrorBoundary"
			>
				<h1 style={{ fontSize: '20px', display: 'inline-block', userSelect: 'auto' }}>⚠️ An error occurred while rendering this content.</h1>

				{identifier && (
					<pre>
						<code>Error Reference: {identifier}</code>
					</pre>
				)}

				<p>
					This error likely occurred in <strong>{errorSource}</strong>.
				</p>

				{actionLog.length > 0 && (
					<pre>
						<code>Running actions...{actionLog}</code>
					</pre>
				)}

				<h3>Actions:</h3>
				<div style={{ display: 'block', marginBottom: '5px' }}>
					<button style={btn} onClick={reset}>
						Retry
					</button>
					<button style={btn} onClick={restartSteam}>
						Restart Steam
					</button>
				</div>

				{wasCausedByPlugin && (
					<div style={{ display: 'block', marginBottom: '5px' }}>
						<button style={btn} onClick={disablePlugin}>
							Disable {errorSource}
						</button>
					</div>
				)}

				<h3>Millennium Updates:</h3>
				<div style={{ display: 'block', marginBottom: '5px' }}>
					<select
						style={sel}
						value={channel}
						onChange={(e) => {
							setChannel(e.target.value as UpdateChannel);
							setCheckState('idle');
						}}
					>
						<option value="stable">Stable</option>
						<option value="beta">Beta</option>
					</select>
					<button style={btn} onClick={onCheckForUpdates} disabled={checkState === 'checking'}>
						{checkState === 'checking' ? 'Checking...' : 'Check for updates'}
					</button>
					{checkState === 'up-to-date' && <span style={{ marginLeft: '6px', opacity: 0.7 }}>Millennium is up to date.</span>}
					{checkState === 'error' && <span style={{ marginLeft: '6px', color: '#f88' }}>Update check failed.</span>}
				</div>

				{(checkState === 'found' || update?.hasUpdate) && (
					<div style={{ display: 'block', marginBottom: '5px' }}>
						<p style={{ margin: '0 0 4px' }}>Millennium {update?.newVersion?.tag_name} is available — updating may resolve this error.</p>
						{isWindows && (
							<button style={btn} onClick={onUpdateMillennium}>
								Update Millennium and restart Steam
							</button>
						)}
						{isLinuxOrMac && startConfig?.millenniumLinuxUpdateScript && (
							<pre style={{ userSelect: 'auto', margin: '4px 0' }}>
								<code>{startConfig.millenniumLinuxUpdateScript}</code>
							</pre>
						)}
					</div>
				)}

				<pre style={{ marginTop: '15px', opacity: 0.7, userSelect: 'auto' }}>
					<code>
						{error.error.stack}
						{'\n\nComponent Stack:'}
						{error.info.componentStack}
					</code>
				</pre>
			</div>
		</>
	);
};

export default MillenniumErrorBoundary;
