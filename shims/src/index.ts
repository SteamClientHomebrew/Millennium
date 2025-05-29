import React from 'react';
import { Millennium } from './Millennium';
import Logger from './logger';

const logger = new Logger('Core');
const isClient = window.location.hostname === 'steamloopback.host';
const startTime = performance.now(); // Start timing

declare global {
	interface Window {
		MILLENNIUM_API: object;
		SP_REACTDOM: any;
		MILLENNIUM_IPC_PORT: number;
	}
}

const CreateWebSocket = (url: string): Promise<WebSocket> => {
	return new Promise((resolve) => {
		try {
			let socket = new WebSocket(url);

			socket.addEventListener('open', () => {
				const endTime = performance.now(); // End timing
				const connectionTime = endTime - startTime;
				logger.log(`Successfully connected to IPC server in ${connectionTime.toFixed(2)} ms.`);
				resolve(socket);
			});

			socket.addEventListener('error', () => {
				console.error('Failed to connect to IPC server:', url);
				window.location.reload(); // Reload the page if the connection fails
			});
		} catch (error) {
			console.warn('Failed to connect to IPC server:', error);
		}
	});
};

const InjectLegacyReactGlobals = async () => {
	logger.log('Injecting Millennium API...');

	window.SP_REACT = {} as any; // create initial object reference.
	window.SP_REACTDOM = {} as any; // create initial object reference.

	Object.assign((window.MILLENNIUM_API ??= {}), await import('@steambrew/client'), await import('./Millennium'));
};

const WaitForSPReactDOM = () => {
	return new Promise<void>((resolve) => {
		const CallBack = () => {
			// @ts-expect-error
			if (window.App?.BFinishedInitStageOne()) {
				InjectLegacyReactGlobals();
				clearInterval(interval);
				resolve();
			}
		};

		const interval = setInterval(() => CallBack(), 0);
	});
};

const AddStyleSheetFromText = (document: Document, innerStyle: string, id?: string) => {
	if (document.querySelectorAll(`style[id='${id}']`).length) return;

	document.head.appendChild(Object.assign(document.createElement('style'), { id: id })).innerText = innerStyle;
};

type SystemColors = Record<string, string>;

const AppendAccentColor = async () => {
	const systemColors: SystemColors = JSON.parse(await Millennium.callServerMethod('core', 'GetSystemColors'));

	const entries = Object.entries(systemColors)
		.map(([key, value]) => {
			const formattedKey = formatCssVarKey(key);
			return `--SystemAccentColor${formattedKey}: ${value};`;
		})
		.join('\n');

	AddStyleSheetFromText(document, `:root {\n${entries}\n}`, 'SystemAccentColorInject');
};

function formatCssVarKey(key: string): string {
	// Capitalize first letter and convert "Rgb" to "-RGB"
	return key
		.replace(/Rgb$/, '-RGB') // turn "Rgb" into "-RGB"
		.replace(/^./, (c) => c.toUpperCase()); // capitalize first letter
}

const StartPreloader = async (port: number, shimList?: string[]) => {
	logger.log(`Successfully bound to ${isClient ? 'client' : 'webkit'} DOM...`);
	const socket = await CreateWebSocket('ws://127.0.0.1:' + port);

	/** Setup IPC */
	window.MILLENNIUM_IPC_PORT = port;
	window.MILLENNIUM_IPC_SOCKET = socket;
	window.CURRENT_IPC_CALL_COUNT = 0;

	if (isClient) {
		await WaitForSPReactDOM();
	} else {
		/** Webkit */
		window.MILLENNIUM_API = await import('./Millennium');
		AppendAccentColor();
	}

	/** Inject the JavaScript shims into the DOM */
	shimList?.forEach(
		(shim) =>
			!document.querySelector(`script[src="${shim}"][type="module"]`) &&
			document.head.appendChild(
				Object.assign(document.createElement('script'), {
					src: shim,
					type: 'module',
					id: 'millennium-injected',
				}),
			),
	);

	const endTime = performance.now(); // End timing
	const connectionTime = endTime - startTime;
	logger.log(`Successfully injected shims into the DOM in ${connectionTime.toFixed(2)} ms.`);
};

export default StartPreloader;
