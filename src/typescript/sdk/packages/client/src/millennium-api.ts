import ProtocolMapping from 'devtools-protocol/types/protocol-mapping';

/** Returnable IPC types */
type IPCType = string | number | boolean | void;

type Json = string | number | boolean | null | void | Json[] | { [key: string]: Json };

/*
 Global Millennium API for developers.
*/
export type QuickAccessTarget = { plugin: string } | { theme: string };

type Millennium = {
	/**
	 * Call a method on the backend
	 * @deprecated Use `callable` instead.
	 * Example usage:
	 * ```typescript
	 * // before
	 * const result = await Millennium.callServerMethod('methodName', { arg1: 'value' });
	 * // after
	 * const method = callable<[{ arg1: string }]>("methodName");
	 *
	 * const result1 = await method({ arg1: 'value1' });
	 * const result2 = await method({ arg1: 'value2' });
	 * ```
	 */
	callServerMethod: (methodName: string, kwargs?: object) => Promise<any>;
	findElement: (privateDocument: Document, querySelector: string, timeOut?: number) => Promise<NodeListOf<Element>>;
	exposeObj: <T extends object>(obj: T) => any;
	AddWindowCreateHook?: (callback: (context: object) => void) => void;

	/**
	 * ```typescript
	 * Millennium.openQuickAccess(); // open QAM home
	 * Millennium.openQuickAccess({ theme: 'themeName' }); // "name" property from the themes skin.json
	 * Millennium.openQuickAccess({ plugin: 'pluginName' }); // "name" property from the plugins plugin.json
	 * ```
	 * @param target
	 * @returns
	 */
	openQuickAccess: (target?: QuickAccessTarget) => Promise<void>;
};

/**
 * Make reusable IPC call declarations
 *
 * frontend:
 * ```typescript
 * const method = callable<[{ arg1: string }]>("methodName"); // declare the method
 * method({ arg1: 'value' }); // call the method
 * ```
 *
 * backend:
 * ```lua
 * function methodName(arg1)
 *     body
 * end
 * ```
 */
const callable: <
	// Ideally this would be `Params extends Record<...>` but for backwards compatibility we keep a tuple type
	Params extends [params: Record<string, IPCType>] | [] = [],
	Return extends IPCType = IPCType,
>(
	route: string,
) => (...params: Params) => Promise<Return> =
	(_route: string) =>
	(..._params: any[]) =>
		Promise.resolve(undefined as any);

/**
 * Like `callable`, but allows JSON objects as positional arguments and auto-parses the JSON result.
 *
 * frontend:
 * ```typescript
 * const method = ffi<[string], MyObject>("methodName");
 * const result = await method('value'); // result is MyObject, not a JSON string
 * ```
 */
const ffi: <Params extends Json[] = [], Return = Json | void>(route: string) => (...params: Params) => Promise<Return> =
	(_route: string) =>
	(..._params: any[]) =>
		Promise.resolve(undefined as any);

const m_private_context: any = undefined;
export const pluginSelf = m_private_context;

const CDP_PROXY_BINDING = '__millennium_cdp_proxy__';
const CDP_EXTENSION_BINDING = '__millennium_extension_route__';
const CDP_EXT_RESP = 'MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE';

declare global {
	interface Window {
		Millennium: Millennium;
		[CDP_EXT_RESP]: { __handleCDPResponse: (response: any) => void };
		[CDP_EXTENSION_BINDING]: (message: string) => void;
		[CDP_PROXY_BINDING]: (message: string) => void;
		__millennium_cdp_resolve__: (callbackId: number, result: any) => void;
		__millennium_cdp_reject__: (callbackId: number, error: string) => void;
		__millennium_cdp_event__: (data: any) => void;
	}
}

const BindPluginSettings: () => any = (): any => undefined;

const pluginConfig: {
	get: <T = any>(key: string) => Promise<T>;
	set: (key: string, value: any) => Promise<void>;
	delete: (key: string) => Promise<void>;
	getAll: <T = Record<string, any>>() => Promise<T>;
} = { get: async () => undefined as any, set: async () => {}, delete: async () => {}, getAll: async () => ({}) as any };

const usePluginConfig: {
	<T = any>(key: string): [T | undefined, (value: T) => Promise<void>];
	(): [Record<string, any>, (key: string, value: any) => Promise<void>];
} = (() => [undefined, async () => {}]) as any;

const subscribePluginConfig: (cb: (key: string, value: any) => void) => () => void = () => () => {};

let _nextId = 0;
const _pending = new Map<number, { resolve: (value: any) => void; reject: (reason?: any) => void; callSite: Error }>();
const _eventDispatchers = new Set<(data: any) => void>();
let _busInitialized = false;

function rejectWithCallSite(pending: { reject: (reason?: any) => void; callSite: Error }, message: string) {
	pending.callSite.message = message;
	/* V8 bakes the message into .stack, so setting .message alone won't update the prefix */
	const stack = pending.callSite.stack ?? '';
	const nl = stack.indexOf('\n');
	pending.callSite.stack = `Error: ${message}` + (nl >= 0 ? stack.slice(nl) : '');
	pending.reject(pending.callSite);
}

function initializeCDPBus() {
	if (_busInitialized) return;
	_busInitialized = true;

	window.__millennium_cdp_resolve__ = (callbackId: number, result: any) => {
		const pending = _pending.get(callbackId);
		if (pending) {
			_pending.delete(callbackId);
			pending.resolve(result ?? {});
		}
	};

	window.__millennium_cdp_reject__ = (callbackId: number, error: string) => {
		const pending = _pending.get(callbackId);
		if (pending) {
			_pending.delete(callbackId);
			rejectWithCallSite(pending, `CDP Error: ${error}`);
		}
	};

	window.__millennium_cdp_event__ = (data: any) => {
		for (const cb of _eventDispatchers) {
			try {
				cb(data);
			} catch (_) {}
		}
	};

	window[CDP_EXT_RESP] = {
		__handleCDPResponse: (response: any) => {
			if (response.id !== undefined) {
				const pending = _pending.get(response.id);
				if (pending) {
					_pending.delete(response.id);
					if (response.error) {
						rejectWithCallSite(pending, `CDP Error: ${response.error.message}`);
					} else {
						pending.resolve(response.result ?? {});
					}
				}
				return;
			}
			if (response.method !== undefined) {
				for (const cb of _eventDispatchers) {
					try {
						cb(response);
					} catch (_) {}
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
					try {
						listener(params);
					} catch (_) {}
				}
			}
		};

		initializeCDPBus();
		_eventDispatchers.add(this._eventDispatcher);
	}

	on<T extends keyof ProtocolMapping.Events>(event: T, listener: (params: ProtocolMapping.Events[T][0] & { sessionId?: string }) => void): () => void {
		const isFirst = !this.eventListeners.has(event) || this.eventListeners.get(event)!.size === 0;
		if (!this.eventListeners.has(event)) {
			this.eventListeners.set(event, new Set());
		}
		this.eventListeners.get(event)!.add(listener);

		if (isFirst) {
			try {
				window[CDP_PROXY_BINDING](JSON.stringify({ action: 'subscribe', pluginName: this._pluginName, event }));
			} catch (_) {}
		}

		return () => this.off(event, listener);
	}

	off<T extends keyof ProtocolMapping.Events>(event: T, listener: (params: ProtocolMapping.Events[T][0] & { sessionId?: string }) => void): void {
		const listeners = this.eventListeners.get(event);
		if (listeners) {
			listeners.delete(listener);
			if (listeners.size === 0) {
				this.eventListeners.delete(event);
				try {
					window[CDP_PROXY_BINDING](JSON.stringify({ action: 'unsubscribe', pluginName: this._pluginName, event }));
				} catch (_) {}
			}
		}
	}

	send<T extends keyof ProtocolMapping.Commands>(
		method: T,
		params: ProtocolMapping.Commands[T]['paramsType'][0] = {} as any,
		sessionId?: string,
	): Promise<ProtocolMapping.Commands[T]['returnType']> {
		if (method.startsWith('Extensions.')) {
			return this._sendViaExtensionRoute(method, params, sessionId);
		}

		const callSite = new Error();
		return new Promise((resolve, reject) => {
			const callbackId = _nextId++;
			_pending.set(callbackId, { resolve, reject, callSite });

			const payload: any = { action: 'cdp_call', pluginName: this._pluginName, callbackId, method };
			if (params && Object.keys(params as object).length > 0) {
				payload.params = params;
			}
			if (sessionId) {
				payload.sessionId = sessionId;
			}

			try {
				window[CDP_PROXY_BINDING](JSON.stringify(payload));
			} catch (error) {
				_pending.delete(callbackId);
				reject(error);
			}
		});
	}

	protected _sendViaExtensionRoute<T extends keyof ProtocolMapping.Commands>(
		method: T,
		params: ProtocolMapping.Commands[T]['paramsType'][0],
		sessionId?: string,
	): Promise<ProtocolMapping.Commands[T]['returnType']> {
		const callSite = new Error();
		return new Promise((resolve, reject) => {
			const id = _nextId++;
			_pending.set(id, { resolve, reject, callSite });

			const payload: any = { id, method };
			if (params && Object.keys(params as object).length > 0) {
				payload.params = params;
			}
			if (sessionId) {
				payload.sessionId = sessionId;
			}

			try {
				window[CDP_EXTENSION_BINDING](JSON.stringify(payload));
			} catch (error) {
				_pending.delete(id);
				reject(error);
			}
		});
	}
}

/* backwards compat with old callers (without requiring recompile with new @steambrew/ttc version). falls back to Millenniums internal CDP. */
class MillenniumChromeDevToolsProtocolShared extends MillenniumChromeDevToolsProtocol {
	constructor() {
		super('__millennium_internal__');
	}

	override send<T extends keyof ProtocolMapping.Commands>(
		method: T,
		params: ProtocolMapping.Commands[T]['paramsType'][0] = {} as any,
		sessionId?: string,
	): Promise<ProtocolMapping.Commands[T]['returnType']> {
		return this._sendViaExtensionRoute(method, params, sessionId);
	}
}

const ChromeDevToolsProtocol: MillenniumChromeDevToolsProtocol = new MillenniumChromeDevToolsProtocolShared();
const Millennium: Millennium = window.Millennium;
export { BindPluginSettings, callable, ffi, ChromeDevToolsProtocol, Millennium, pluginConfig, subscribePluginConfig, usePluginConfig };
