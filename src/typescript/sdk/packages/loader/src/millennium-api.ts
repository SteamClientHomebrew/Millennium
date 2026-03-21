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
		/**
		 * Only handle FRONT_END_LOADED (id=1). Old compiled plugins may still call
		 * postMessage(0, ...) for CALL_SERVER_METHOD (now handled by ffiBinder.call),
		 * so we must ignore those to avoid sending empty-method calls.
		 */
		if (messageId !== 1) return { returnValue: null };
		const ffi = window.MILLENNIUM_PRIVATE_INTERNAL_FOREIGN_FUNCTION_INTERFACE_DO_NOT_USE;
		if (!ffi) return { returnValue: null };
		return ffi.call(messageId, contents.pluginName ?? '', '', []);
	},
};

window.MILLENNIUM_BACKEND_IPC = backendIPC;

enum IpcMethod {
	CALL_SERVER_METHOD,
	FRONT_END_LOADED,
	CALL_FRONTEND_METHOD,
	PLUGIN_CONFIG,
}

enum ConfigMethod {
	GET,
	SET,
	DELETE,
	GET_ALL,
	DELETE_ALL,
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
		}
	>();
	private nextId = 0;

	/** Capture the full call stack. */
	private getCaller(): string {
		try {
			const stack = new Error().stack;
			if (!stack) return '';
			// Drop the "Error" prefix line, keep all "at ..." frames.
			return stack.split('\n').filter(l => l.trim().startsWith('at ')).map(l => l.trim()).join('\n');
		} catch { /* noop */ }
		return '';
	}

	async call(payloadType: IpcMethod, pluginName: string, methodName: string | number, argumentList: any): Promise<any> {
		if (typeof (window as any).__private_millennium_ffi_do_not_use__ !== 'function') {
			console.error("Millennium FFI is not available in this context. To use the FFI, make sure you've selected the 'millennium' context");
			return;
		}

		const requestId = this.nextId++;
		const caller = this.getCaller();

		return new Promise((resolve, reject) => {
			this.pendingRequests.set(requestId, { resolve, reject });
			(window as any).__private_millennium_ffi_do_not_use__(
				JSON.stringify({
					id: payloadType,
					call_id: requestId,
					caller,
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

		this.pendingRequests.delete(requestId);
		if (response.success) {
			pending.resolve(JSON.stringify(response.returnJson));
		} else {
			pending.reject(new Error('Millennium Error: ' + (response.returnJson || 'Unknown error') + '\n\n\nMillennium internal traceback'));
		}
	}

	destroy() {
		this.pendingRequests.forEach((pending) => {
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

// Callable wrapper — creates a reusable RPC function for a given route.
export function callable(pluginNameOrFn: string | ((...args: any[]) => any), route: string) {
	if (typeof pluginNameOrFn === 'function') {
		// Old API: callable(__call_server_method__, route)
		// __call_server_method__(route, ...args) → Millennium.callServerMethod(pluginName, route, ...args)
		return (...args: any[]) => pluginNameOrFn(route, ...args);
	}
	return (...args: any[]) => Millennium.callServerMethod(pluginNameOrFn, route, ...args);
}

// Only define pluginSelf if on loopback host
const m_private_context: any = undefined;
export const pluginSelf = isClient ? m_private_context : undefined;

export const BindPluginSettings = (pluginName: string) => window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[pluginName]?.settingsStore;

type ConfigChangeCallback = (key: string, value: any) => void;
const _pluginConfigListeners = new Map<string, Set<ConfigChangeCallback>>();

(window as any).__millennium_plugin_config_changed__ = (
	pluginName: string,
	key: string,
	valueJson: string,
) => {
	try {
		const listeners = _pluginConfigListeners.get(pluginName);
		if (!listeners) return;
		const value = JSON.parse(valueJson);
		listeners.forEach(cb => cb(key, value));
	} catch (e) {
		console.error('[Millennium] Config change error:', e);
	}
};

export function subscribePluginConfig(pluginName: string, cb: ConfigChangeCallback): () => void {
	let set = _pluginConfigListeners.get(pluginName);
	if (!set) {
		set = new Set();
		_pluginConfigListeners.set(pluginName, set);
	}
	set.add(cb);
	return () => {
		set!.delete(cb);
		if (set!.size === 0) _pluginConfigListeners.delete(pluginName);
	};
}

async function _configCall(pluginName: string, method: ConfigMethod, args: Record<string, any> = {}) {
	const raw = await ffiBinder.call(IpcMethod.PLUGIN_CONFIG, pluginName, method, args);
	return JSON.parse(raw);
}

export const pluginConfig = {
	get: <T = any>(pluginName: string, key: string): Promise<T> =>
		_configCall(pluginName, ConfigMethod.GET, { key }),

	set: (pluginName: string, key: string, value: any): Promise<void> =>
		_configCall(pluginName, ConfigMethod.SET, { key, value }),

	delete: (pluginName: string, key: string): Promise<void> =>
		_configCall(pluginName, ConfigMethod.DELETE, { key }),

	getAll: <T = Record<string, any>>(pluginName: string): Promise<T> =>
		_configCall(pluginName, ConfigMethod.GET_ALL, {}),
};

const _React = () => (window as any).SP_REACT;

export function usePluginConfig<T = any>(pluginName: string, key?: string): [T | undefined, (...args: any[]) => Promise<void>] {
	const React = _React();
	const [state, setState] = React.useState(undefined as T | undefined);

	React.useEffect(() => {
		let cancelled = false;

		const fetchInitial = async () => {
			try {
				const result = key
					? await pluginConfig.get<T>(pluginName, key)
					: await pluginConfig.getAll(pluginName);
				if (!cancelled) setState(result as T);
			} catch {
				/* initial fetch failed — leave as undefined */
			}
		};

		fetchInitial();

		const unsub = subscribePluginConfig(pluginName, (changedKey, value) => {
			if (cancelled) return;
			if (key) {
				if (changedKey === key) setState(value);
			} else {
				setState((prev: any) => ({ ...prev, [changedKey]: value }));
			}
		});

		return () => {
			cancelled = true;
			unsub();
		};
	}, [pluginName, key]);

	const setter = React.useCallback(
		key
			? async (value: T) => {
					await pluginConfig.set(pluginName, key, value);
					setState(value);
				}
			: async (k: string, v: any) => {
					await pluginConfig.set(pluginName, k, v);
					setState((prev: any) => ({ ...prev, [k]: v }));
				},
		[pluginName, key],
	);

	return [state, setter];
}
