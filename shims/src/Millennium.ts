const isClient = window.location.hostname === 'steamloopback.host';

declare global {
	interface Window {
		Millennium: any;
		PLUGIN_LIST: any;
		MILLENNIUM_BACKEND_IPC: typeof backendIPC;
		MILLENNIUM_IPC_SOCKET: WebSocket;
		CURRENT_IPC_CALL_COUNT: number;
		MILLENNIUM_PLUGIN_SETTINGS_STORE: any;
	}
}

type Json = string | number | boolean | null | Json[] | { [key: string]: Json };
type IPC_types = string | number | boolean | Json;
declare const g_PopupManager: any;

const backendIPC = {
	postMessage: (messageId: number, contents: any): Promise<any> =>
		new Promise((resolve) => {
			const iteration = window.CURRENT_IPC_CALL_COUNT++;
			const message = { id: messageId, iteration, data: contents };

			const handler = (event: MessageEvent) => {
				try {
					const json = JSON.parse(event.data);
					if (json.id !== iteration) {
						return;
					}

					window.MILLENNIUM_IPC_SOCKET.removeEventListener('message', handler);
					resolve(json);
				} catch {
					console.error('Invalid JSON from IPC:', event.data, contents);
				}
			};

			window.MILLENNIUM_IPC_SOCKET.addEventListener('message', handler);
			window.MILLENNIUM_IPC_SOCKET.send(JSON.stringify(message));
		}),
};

window.MILLENNIUM_BACKEND_IPC = backendIPC;

export const Millennium = {
	callServerMethod: (pluginName: string, methodName: string, kwargs?: any) => {
		const query = { pluginName, methodName, ...(kwargs && { argumentList: kwargs }) };

		return backendIPC.postMessage(0, query).then((res: any) => {
			return typeof res.returnValue === 'string' ? atob(res.returnValue) : res.returnValue;
		});
	},

	findElement: (doc: Document, selector: string, timeout?: number): Promise<NodeListOf<Element>> =>
		new Promise((resolve, reject) => {
			const found = doc.querySelectorAll(selector);
			if (found.length) return resolve(found);

			const observer = new MutationObserver(() => {
				const match = doc.querySelectorAll(selector);
				if (match.length) {
					observer.disconnect();
					timer && clearTimeout(timer);
					resolve(match);
				}
			});

			observer.observe(doc.body, { childList: true, subtree: true });

			const timer =
				timeout &&
				setTimeout(() => {
					observer.disconnect();
					reject();
				}, timeout);
		}),

	...(isClient && {
		exposeObj: <T extends object>(exports: any, obj: T) => Object.assign(exports, obj),
		AddWindowCreateHook: (cb: any) => g_PopupManager.AddPopupCreatedCallback(cb),
	}),
};

window.Millennium = Millennium;

// Callable wrapper
export const callable =
	<Args extends any[] = [], Return = void | IPC_types>(cb: (route: string, ...args: Args) => Promise<Return>, route: string): ((...args: Args) => Promise<Return>) =>
	(...args: Args) =>
		cb(route, ...args);

// Only define pluginSelf if on loopback host
const m_private_context: any = undefined;
export const pluginSelf = isClient ? m_private_context : undefined;

export const BindPluginSettings = (pluginName: string) => window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[pluginName]?.settingsStore;
