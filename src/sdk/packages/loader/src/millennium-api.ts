const isClient = window.location.hostname === 'steamloopback.host';

declare global {
	interface Window {
		PLUGIN_LIST: any;
		MILLENNIUM_BACKEND_IPC: typeof backendIPC;
		MILLENNIUM_IPC_SOCKET: WebSocket;
		CURRENT_IPC_CALL_COUNT: number;
		MILLENNIUM_PLUGIN_SETTINGS_STORE: any;
		MILLENNIUM_API: any;
		MILLENNIUM_PRIVATE_INTERNAL_FOREIGN_FUNCTION_INTERFACE_DO_NOT_USE: FFI_Binder;
	}
}

type Json = string | number | boolean | null | Json[] | { [key: string]: Json };
type IPC_types = string | number | boolean | Json;
declare const g_PopupManager: any;

let millenniumAuthToken: string | undefined = undefined;

export function setMillenniumAuthToken(token: string) {
	millenniumAuthToken = token;
}

const backendIPC = {
	postMessage: async (messageId: number, contents: any): Promise<any> => {
		return new Promise((resolve) => {
			resolve({
				returnValue: null,
			});
		});
	},
};

window.MILLENNIUM_BACKEND_IPC = backendIPC;

enum IpcMethod {
	CALL_SERVER_METHOD,
	FRONT_END_LOADED,
	CALL_FRONTEND_METHOD,
}

interface ResponsePayload {
	success: boolean;
	returnJson?: any;
	error?: string;
}

class FFI_Binder {
	private pendingRequests = new Map<
		number,
		{
			resolve: (value: any) => void;
			reject: (error: Error) => void;
			timeout: NodeJS.Timeout;
		}
	>();
	private nextId = 0;
	private requestTimeout = 5000;

	async call(payloadType: IpcMethod, pluginName: string, methodName: string, argumentList: any[]): Promise<any> {
		if (!window.hasOwnProperty('__private_millennium_ffi_do_not_use__')) {
			console.error("Millennium FFI is not available in this context. To use the FFI, make sure you've selected the 'millennium' context");
			return;
		}

		const requestId = this.nextId++;

		return new Promise((resolve, reject) => {
			const timeout = setTimeout(() => {
				this.pendingRequests.delete(requestId);
				reject(new Error(`Request ${requestId} timed out`));
			}, this.requestTimeout);
			this.pendingRequests.set(requestId, { resolve, reject, timeout });
			(window as any).__private_millennium_ffi_do_not_use__(
				JSON.stringify({
					id: payloadType,
					call_id: requestId,
					data: {
						pluginName,
						methodName,
						argumentList,
					},
				}),
			);
		});
	}

	__handleResponse(requestId: number, response: ResponsePayload) {
		const pending = this.pendingRequests.get(requestId);
		if (!pending) {
			console.warn('[Millennium] No pending request for', requestId, response);
			return;
		}
		clearTimeout(pending.timeout);
		this.pendingRequests.delete(requestId);
		if (response.success) {
			pending.resolve(response.returnJson);
		} else {
			pending.reject(new Error('Millennium Error: ' + (response.returnJson || 'Unknown error') + '\n\n\nMillennium internal traceback'));
		}
	}

	destroy() {
		this.pendingRequests.forEach((pending) => {
			clearTimeout(pending.timeout);
			pending.reject(new Error('Manager destroyed'));
		});
		this.pendingRequests.clear();
	}
}

const ffiBinder = new FFI_Binder();
window.MILLENNIUM_PRIVATE_INTERNAL_FOREIGN_FUNCTION_INTERFACE_DO_NOT_USE = ffiBinder;

export const Millennium = {
	callServerMethod: (pluginName: string, methodName: string, kwargs?: any) => {
		if (methodName.startsWith('frontend:')) {
			return ffiBinder.call(IpcMethod.CALL_FRONTEND_METHOD, pluginName, methodName.substring(9), kwargs);
		}
		return ffiBinder.call(IpcMethod.CALL_SERVER_METHOD, pluginName, methodName, kwargs);
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

	exposeObj: <T extends object>(exports: any, obj: T) => Object.assign(exports, obj),
	...(isClient && {
		AddWindowCreateHook: (cb: any) => {
			Array.from(g_PopupManager.GetPopups()).forEach(cb);
			g_PopupManager.AddPopupCreatedCallback(cb);
		},
	}),
};

// @ts-expect-error. The types don't have pluginName on callServerMethod, but it is used in the client.
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
