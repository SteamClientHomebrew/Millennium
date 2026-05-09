import { constSysfsExpr } from './constSysfsExpr';

declare global {
	interface Window {
		Millennium: Millennium;
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
	exposeObj: <T extends object>(obj: T) => any;
};

type Json = string | number | boolean | null | Json[] | { [key: string]: Json };

// callable function definition
declare const callable: <Args extends any[] = [], T = IPC_types>(route: string) => (...args: Args) => Promise<T>;

declare const ffi: <Args extends Json[] = [], T extends Json | void = Json>(route: string) => (...args: Args) => Promise<T>;

declare global {
	interface Window {
		Millennium: Millennium;
	}
}

declare const BindPluginSettings: () => any;

const Millennium: Millennium = window.Millennium;
export { BindPluginSettings, callable, ffi, constSysfsExpr, Millennium };
