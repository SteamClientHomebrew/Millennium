import ProtocolMapping from 'devtools-protocol/types/protocol-mapping';

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

declare const g_PopupManager: any;

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
			callSite: Error;
			raw?: boolean;
		}
	>();
	private nextId = 0;

	private static rejectWithCallSite(pending: { reject: (reason?: any) => void; callSite: Error }, message: string) {
		pending.callSite.message = message;
		const stack = pending.callSite.stack ?? '';
		const nl = stack.indexOf('\n');
		pending.callSite.stack = `Error: ${message}` + (nl >= 0 ? stack.slice(nl) : '');
		pending.reject(pending.callSite);
	}

	/** Capture the full call stack. */
	private getCaller(): string {
		try {
			const stack = new Error().stack;
			if (!stack) return '';
			// Drop the "Error" prefix line, keep all "at ..." frames.
			return stack
				.split('\n')
				.filter((l) => l.trim().startsWith('at '))
				.map((l) => l.trim())
				.join('\n');
		} catch {
			/* noop */
		}
		return '';
	}

	async call(payloadType: IpcMethod, pluginName: string, methodName: string | number, argumentList: any, callSite?: Error, raw?: boolean): Promise<any> {
		if (typeof (window as any).__private_millennium_ffi_do_not_use__ !== 'function') {
			console.error("Millennium FFI is not available in this context. To use the FFI, make sure you've selected the 'millennium' context");
			return;
		}

		const requestId = this.nextId++;
		if (!callSite) callSite = new Error();
		const caller = this.getCaller();

		return new Promise((resolve, reject) => {
			this.pendingRequests.set(requestId, { resolve, reject, callSite, raw });
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
			pending.resolve(pending.raw ? (response.returnJson ?? null) : JSON.stringify(response.returnJson));
		} else {
			FFI_Binder.rejectWithCallSite(pending, 'Millennium Error: ' + (response.returnJson || 'Unknown error'));
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
	callServerMethod: (pluginName: string, methodName: string, kwargs?: any, callSite?: Error) => {
		if (methodName.startsWith('frontend:')) {
			return ffiBinder.call(IpcMethod.CALL_FRONTEND_METHOD, pluginName, methodName.substring(9), kwargs, callSite);
		}
		return ffiBinder.call(IpcMethod.CALL_SERVER_METHOD, pluginName, methodName, kwargs, callSite);
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

	exposeObj: <T extends object>(exportsOrObj: any, obj?: T) => obj !== undefined ? Object.assign(exportsOrObj, obj) : exportsOrObj,

	openQuickAccess: async (target?: { plugin: string } | { theme: string }): Promise<void> => {
		await _frontendApiReady;
		(window.MILLENNIUM_API as any).openQuickAccess(target);
	},

	...(isClient && {
		AddWindowCreateHook: (cb: any) => {
			Array.from(g_PopupManager.GetPopups()).forEach(cb);
			g_PopupManager.AddPopupCreatedCallback(cb);
		},
	}),
};

// @ts-expect-error. The types don't have pluginName on callServerMethod, but it is used in the client.
window.Millennium = Millennium;

export function callable<Params extends [params: Record<string, unknown>] | [] = [], Return = unknown>(route: string): (...params: Params) => Promise<Return>;
export function callable<Params extends [params: Record<string, unknown>] | [] = [], Return = unknown>(pluginName: string, route: string): (...params: Params) => Promise<Return>;
export function callable(pluginNameOrRoute: string | ((...args: any[]) => any), route?: string): (...args: any[]) => Promise<any> {
	if (typeof pluginNameOrRoute === 'function') {
		return (...args: any[]) => pluginNameOrRoute(route!, ...args);
	}
	const [pname, r] = route !== undefined ? [pluginNameOrRoute, route] : ['', pluginNameOrRoute];
	return (...args: any[]) => {
		const callSite = new Error();
		return Millennium.callServerMethod(pname, r, args[0], callSite);
	};
}

export function ffi<Params extends unknown[] = unknown[], Return = unknown>(route: string): (...params: Params) => Promise<Return>;
export function ffi<Params extends unknown[] = unknown[], Return = unknown>(pluginName: string, route: string): (...params: Params) => Promise<Return>;
export function ffi(pluginNameOrRoute: string, route?: string): (...args: any[]) => Promise<any> {
	const [pname, r] = route !== undefined ? [pluginNameOrRoute, route] : ['', pluginNameOrRoute];
	return (...args: any[]) => {
		const callSite = new Error();
		const frontend = r.startsWith('frontend:');
		const method = frontend ? IpcMethod.CALL_FRONTEND_METHOD : IpcMethod.CALL_SERVER_METHOD;
		const methodName = frontend ? r.substring(9) : r;
		return ffiBinder.call(method, pname, methodName, args, callSite, true);
	};
}

// Only define pluginSelf if on loopback host
const m_private_context: any = undefined;
export const pluginSelf = isClient ? m_private_context : undefined;

export function BindPluginSettings(): any;
export function BindPluginSettings(pluginName: string): any;
export function BindPluginSettings(pluginName?: string): any {
	return window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[pluginName ?? '']?.settingsStore;
}

type ConfigChangeCallback = (key: string, value: any) => void;
const _pluginConfigListeners = new Map<string, Set<ConfigChangeCallback>>();

(window as any).__millennium_plugin_config_changed__ = (pluginName: string, key: string, valueJson: string) => {
	try {
		const listeners = _pluginConfigListeners.get(pluginName);
		if (!listeners) return;
		const value = JSON.parse(valueJson);
		listeners.forEach((cb) => cb(key, value));
	} catch (e) {
		console.error('[Millennium] Config change error:', e);
	}
};

export function subscribePluginConfig(cb: ConfigChangeCallback): () => void;
export function subscribePluginConfig(pluginName: string, cb: ConfigChangeCallback): () => void;
export function subscribePluginConfig(pluginNameOrCb: string | ConfigChangeCallback, cb?: ConfigChangeCallback): () => void {
	const [pluginName, callback] = typeof pluginNameOrCb === 'function' ? ['', pluginNameOrCb] : [pluginNameOrCb, cb!];
	let set = _pluginConfigListeners.get(pluginName);
	if (!set) {
		set = new Set();
		_pluginConfigListeners.set(pluginName, set);
	}
	set.add(callback);
	return () => {
		set!.delete(callback);
		if (set!.size === 0) _pluginConfigListeners.delete(pluginName);
	};
}

async function _configCall(pluginName: string, method: ConfigMethod, args: Record<string, any> = {}) {
	const raw = await ffiBinder.call(IpcMethod.PLUGIN_CONFIG, pluginName, method, args);
	return JSON.parse(raw);
}

export const pluginConfig = {
	get: function<T = any>(pluginNameOrKey: string, key?: string): Promise<T> {
		return key !== undefined ? _configCall(pluginNameOrKey, ConfigMethod.GET, { key }) : _configCall('', ConfigMethod.GET, { key: pluginNameOrKey });
	},
	set: function(pluginNameOrKey: string, keyOrValue?: string, value?: any): Promise<void> {
		return keyOrValue !== undefined && value !== undefined
			? _configCall(pluginNameOrKey, ConfigMethod.SET, { key: keyOrValue, value })
			: _configCall('', ConfigMethod.SET, { key: pluginNameOrKey, value: keyOrValue });
	},
	delete: function(pluginNameOrKey: string, key?: string): Promise<void> {
		return key !== undefined ? _configCall(pluginNameOrKey, ConfigMethod.DELETE, { key }) : _configCall('', ConfigMethod.DELETE, { key: pluginNameOrKey });
	},
	getAll: function<T = Record<string, any>>(pluginName?: string): Promise<T> {
		return _configCall(pluginName ?? '', ConfigMethod.GET_ALL, {});
	},
};

const _frontendApiReady: Promise<void> = new Promise((resolve) => {
	if (typeof window.MILLENNIUM_API?.openQuickAccess === 'function') {
		resolve();
	} else {
		window.addEventListener('millennium-quick-access-ready', () => resolve(), { once: true });
	}
});

const _React = () => (window as any).SP_REACT;

export function usePluginConfig<T = any>(key?: string): [T | undefined, (...args: any[]) => Promise<void>];
export function usePluginConfig<T = any>(pluginName: string, key?: string): [T | undefined, (...args: any[]) => Promise<void>];
export function usePluginConfig<T = any>(pluginNameOrKey?: string, key?: string): [T | undefined, (...args: any[]) => Promise<void>] {
	const [pluginName, resolvedKey] = key !== undefined ? [pluginNameOrKey!, key] : ['', pluginNameOrKey];
	const React = _React();
	const [state, setState] = React.useState(undefined as T | undefined);

	React.useEffect(() => {
		let cancelled = false;

		const fetchInitial = async () => {
			try {
				const result = resolvedKey ? await pluginConfig.get<T>(pluginName, resolvedKey) : await pluginConfig.getAll(pluginName);
				if (!cancelled) setState(result as T);
			} catch {
				/* initial fetch failed — leave as undefined */
			}
		};

		fetchInitial();

		const unsub = subscribePluginConfig(pluginName, (changedKey, value) => {
			if (cancelled) return;
			if (resolvedKey) {
				if (changedKey === resolvedKey) setState(value);
			} else {
				setState((prev: any) => ({ ...prev, [changedKey]: value }));
			}
		});

		return () => {
			cancelled = true;
			unsub();
		};
	}, [pluginName, resolvedKey]);

	const setter = React.useCallback(
		resolvedKey
			? async (value: T) => {
					await pluginConfig.set(pluginName, resolvedKey, value);
					setState(value);
				}
			: async (k: string, v: any) => {
					await pluginConfig.set(pluginName, k, v);
					setState((prev: any) => ({ ...prev, [k]: v }));
				},
		[pluginName, resolvedKey],
	);

	return [state, setter];
}

export type QuickAccessTarget = { plugin: string } | { theme: string };

const CDP_PROXY_BINDING = '__millennium_cdp_proxy__';
const CDP_EXTENSION_BINDING = '__millennium_extension_route__';
const CDP_EXT_RESP = 'MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE';

declare global {
	interface Window {
		[CDP_EXT_RESP]: { __handleCDPResponse: (response: any) => void };
		[CDP_EXTENSION_BINDING]: (message: string) => void;
		[CDP_PROXY_BINDING]: (message: string) => void;
		__millennium_cdp_resolve__: (callbackId: number, result: any) => void;
		__millennium_cdp_reject__: (callbackId: number, error: string) => void;
		__millennium_cdp_event__: (data: any) => void;
	}
}

let _cdpNextId = 0;
const _cdpPending = new Map<number, { resolve: (value: any) => void; reject: (reason?: any) => void; callSite: Error }>();
const _cdpEventDispatchers = new Set<(data: any) => void>();
let _cdpBusInitialized = false;

function _rejectCdpWithCallSite(pending: { reject: (reason?: any) => void; callSite: Error }, message: string) {
	pending.callSite.message = message;
	const stack = pending.callSite.stack ?? '';
	const nl = stack.indexOf('\n');
	pending.callSite.stack = `Error: ${message}` + (nl >= 0 ? stack.slice(nl) : '');
	pending.reject(pending.callSite);
}

function _initializeCDPBus() {
	if (_cdpBusInitialized) return;
	_cdpBusInitialized = true;

	window.__millennium_cdp_resolve__ = (callbackId: number, result: any) => {
		const pending = _cdpPending.get(callbackId);
		if (pending) {
			_cdpPending.delete(callbackId);
			pending.resolve(result ?? {});
		}
	};

	window.__millennium_cdp_reject__ = (callbackId: number, error: string) => {
		const pending = _cdpPending.get(callbackId);
		if (pending) {
			_cdpPending.delete(callbackId);
			_rejectCdpWithCallSite(pending, `CDP Error: ${error}`);
		}
	};

	window.__millennium_cdp_event__ = (data: any) => {
		for (const cb of _cdpEventDispatchers) {
			try { cb(data); } catch (_) {}
		}
	};

	window[CDP_EXT_RESP] = {
		__handleCDPResponse: (response: any) => {
			if (response.id !== undefined) {
				const pending = _cdpPending.get(response.id);
				if (pending) {
					_cdpPending.delete(response.id);
					if (response.error) {
						_rejectCdpWithCallSite(pending, `CDP Error: ${response.error.message}`);
					} else {
						pending.resolve(response.result ?? {});
					}
				}
				return;
			}
			if (response.method !== undefined) {
				for (const cb of _cdpEventDispatchers) {
					try { cb(response); } catch (_) {}
				}
			}
		},
	};
}

export class MillenniumChromeDevToolsProtocol {
	protected readonly _pluginName: string;
	eventListeners: Map<string, Set<(params: any) => void>>;
	private readonly _eventDispatcher: (data: any) => void;

	constructor(pluginName: string) {
		this._pluginName = pluginName;
		this.eventListeners = new Map();

		const eventListeners = this.eventListeners;
		this._eventDispatcher = (data: any) => {
			if (data.method === undefined) return;
			const params = data.sessionId ? { ...data.params, sessionId: data.sessionId } : data.params;
			const listeners = eventListeners.get(data.method);
			if (listeners) {
				for (const listener of listeners) {
					try { listener(params); } catch (_) {}
				}
			}
		};

		_initializeCDPBus();
		_cdpEventDispatchers.add(this._eventDispatcher);
	}

	on<T extends keyof ProtocolMapping.Events>(event: T, listener: (params: ProtocolMapping.Events[T][0] & { sessionId?: string }) => void): () => void {
		const isFirst = !this.eventListeners.has(event) || this.eventListeners.get(event)!.size === 0;
		if (!this.eventListeners.has(event)) this.eventListeners.set(event, new Set());
		this.eventListeners.get(event)!.add(listener);
		if (isFirst) {
			try { window[CDP_PROXY_BINDING](JSON.stringify({ action: 'subscribe', pluginName: this._pluginName, event })); } catch (_) {}
		}
		return () => this.off(event, listener);
	}

	off<T extends keyof ProtocolMapping.Events>(event: T, listener: (params: ProtocolMapping.Events[T][0] & { sessionId?: string }) => void): void {
		const listeners = this.eventListeners.get(event);
		if (listeners) {
			listeners.delete(listener);
			if (listeners.size === 0) {
				this.eventListeners.delete(event);
				try { window[CDP_PROXY_BINDING](JSON.stringify({ action: 'unsubscribe', pluginName: this._pluginName, event })); } catch (_) {}
			}
		}
	}

	send<T extends keyof ProtocolMapping.Commands>(
		method: T,
		params: ProtocolMapping.Commands[T]['paramsType'][0] = {} as any,
		sessionId?: string,
	): Promise<ProtocolMapping.Commands[T]['returnType']> {
		if (method.startsWith('Extensions.')) return this._sendViaExtensionRoute(method, params, sessionId);
		const callSite = new Error();
		return new Promise((resolve, reject) => {
			const callbackId = _cdpNextId++;
			_cdpPending.set(callbackId, { resolve, reject, callSite });
			const payload: any = { action: 'cdp_call', pluginName: this._pluginName, callbackId, method };
			if (params && Object.keys(params as object).length > 0) payload.params = params;
			if (sessionId) payload.sessionId = sessionId;
			try { window[CDP_PROXY_BINDING](JSON.stringify(payload)); } catch (error) { _cdpPending.delete(callbackId); reject(error); }
		});
	}

	protected _sendViaExtensionRoute<T extends keyof ProtocolMapping.Commands>(
		method: T,
		params: ProtocolMapping.Commands[T]['paramsType'][0],
		sessionId?: string,
	): Promise<ProtocolMapping.Commands[T]['returnType']> {
		const callSite = new Error();
		return new Promise((resolve, reject) => {
			const id = _cdpNextId++;
			_cdpPending.set(id, { resolve, reject, callSite });
			const payload: any = { id, method };
			if (params && Object.keys(params as object).length > 0) payload.params = params;
			if (sessionId) payload.sessionId = sessionId;
			try { window[CDP_EXTENSION_BINDING](JSON.stringify(payload)); } catch (error) { _cdpPending.delete(id); reject(error); }
		});
	}
}

class MillenniumChromeDevToolsProtocolShared extends MillenniumChromeDevToolsProtocol {
	constructor() { super('__millennium_internal__'); }

	override send<T extends keyof ProtocolMapping.Commands>(
		method: T,
		params: ProtocolMapping.Commands[T]['paramsType'][0] = {} as any,
		sessionId?: string,
	): Promise<ProtocolMapping.Commands[T]['returnType']> {
		return this._sendViaExtensionRoute(method, params, sessionId);
	}
}

export const ChromeDevToolsProtocol: MillenniumChromeDevToolsProtocol = new MillenniumChromeDevToolsProtocolShared();
