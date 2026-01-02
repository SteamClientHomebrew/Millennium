const isClient = window.location.hostname === 'steamloopback.host';

declare global {
	interface Window {
		PLUGIN_LIST: any;
		MILLENNIUM_BACKEND_IPC: typeof backendIPC;
		MILLENNIUM_IPC_SOCKET: WebSocket;
		CURRENT_IPC_CALL_COUNT: number;
		MILLENNIUM_PLUGIN_SETTINGS_STORE: any;
		MILLENNIUM_API: {
			ChromeDevToolsProtocol: any;
		};
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
		if (!millenniumAuthToken) {
			console.warn('No Millennium auth token set. This will cause issues with IPC calls.');
		}

		const response = await fetch('https://millennium.ipc', {
			method: 'POST',
			headers: {
				'Content-Type': 'text/plain',
				'X-Millennium-Auth': millenniumAuthToken,
			},
			body: JSON.stringify({ id: messageId, data: contents }),
		});

		return await response.json();
	},
};

window.MILLENNIUM_BACKEND_IPC = backendIPC;

export const Millennium = {
	callServerMethod: (pluginName: string, methodName: string, kwargs?: any) => {
		const query = { pluginName, methodName, ...(kwargs && { argumentList: kwargs }) };

		return backendIPC
			.postMessage(0, query)
			.then((res: any) => {
				return typeof res.returnValue === 'string' ? decodeURIComponent(escape(atob(res.returnValue))) : res.returnValue;
			})
			.catch((err: any) => {
				console.error(`Error calling server method ${pluginName}.${methodName}:`, err);
				throw err;
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

export const __INTERNAL_CALL_WEBKIT_METHOD__ = async (pluginName: string, methodName: string, kwargs?: any): Promise<any[]> => {
	/** get all the targets from the main Steam browser instance, which includes the sandboxed web browser used for the steam store, etc. */
	const { targetInfos } = await window.MILLENNIUM_API.ChromeDevToolsProtocol.send('Target.getTargets');

	/**
	 * Filter out targets that are apart of the Steam Client UI
	 * all sandboxed web browser targets have canAccessOpener=false and a valid http(s) url
	 */
	const webKitTargets = targetInfos.filter(
		(e) =>
			!e?.canAccessOpener &&
			e?.url !== '' &&
			(e?.url?.startsWith('http://') || e?.url?.startsWith('https://')) &&
			!e?.url?.startsWith('https://steamloopback.host/index.html'),
	);

	/** return early if we have nothing to execute on */
	if (webKitTargets.length === 0) {
		return [];
	}

	const executeOnTarget = async (target) => {
		const { sessionId } = await window.MILLENNIUM_API.ChromeDevToolsProtocol.send('Target.attachToTarget', {
			targetId: target.targetId,
			flatten: true,
		});

		try {
			/*
			 * Safe from injection attacks: JSON.stringify escapes all special characters in pluginName,
			 * methodName, and args, preventing them from breaking out of string literals or injecting code.
			 *
			 * Even malicious inputs like "'; alert(1); //" are harmless escaped strings like "\"; alert(1); //".
			 * Bracket notation [plugin][method] then safely accesses properties without eval-style execution.
			 */
			const expression = `(async () => {
                const plugin = ${JSON.stringify(pluginName)};
                const method = ${JSON.stringify(methodName)};
                const args = ${JSON.stringify(Object.values(kwargs ?? {}))};
                return await window.PLUGIN_LIST[plugin][method](...args);
            })()`;

			/** run the expression in the target's context */
			const result = await window.MILLENNIUM_API.ChromeDevToolsProtocol.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, sessionId);

			if (result.exceptionDetails) {
				console.error(`Error in ${target.url}:`, result);
				return null;
			}

			return { url: target.url, value: result.result.value };
		} catch (error) {
			console.error(`Failed to execute on ${target.url}:`, error);
			return null;
		} finally {
			await window.MILLENNIUM_API.ChromeDevToolsProtocol.send('Target.detachFromTarget', { sessionId });
		}
	};

	/** execute on all targets in parallel */
	const results = await Promise.all(webKitTargets.map(executeOnTarget));
	return results.filter((r) => r !== null);
};
