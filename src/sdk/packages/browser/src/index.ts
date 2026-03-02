import { constSysfsExpr } from './constSysfsExpr';

declare global {
	interface Window {
		Millennium: Millennium;
		MILLENNIUM_API?: {
			callable?: (...args: any[]) => any;
			BindPluginSettings?: (...args: any[]) => any;
		};
	}
}

/** Returnable IPC types */
type IPC_types = string | number | boolean | void;
/*
 Global Millennium API for developers.
*/
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
};

const callable = <Args extends any[] = [], T = IPC_types>(route: string): ((...args: Args) => Promise<T>) => {
	const runtimeCallable = window.MILLENNIUM_API?.callable;
	if (typeof runtimeCallable === 'function') {
		try {
			const maybeBound = runtimeCallable(route);
			if (typeof maybeBound === 'function') {
				return maybeBound as (...args: Args) => Promise<T>;
			}
		} catch {
			// Fall through to deterministic error path.
		}
	}

	return (...args: Args): Promise<T> => {
		void args;
		return Promise.reject(new Error(`[Millennium] callable("${route}") is unavailable before bootstrap runtime is initialized.`));
	};
};

const BindPluginSettings = (pluginName?: string): any => {
	const runtimeBind = window.MILLENNIUM_API?.BindPluginSettings;
	if (typeof runtimeBind === 'function') {
		return runtimeBind(pluginName);
	}
	return undefined;
};

const Millennium: Millennium = window.Millennium;
export { BindPluginSettings, callable, constSysfsExpr, Millennium };
