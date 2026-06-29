import * as React from 'react';
import { Ref, useState } from 'react';

// this shouldn't need to be redeclared but it does for some reason

declare global {
	interface Window {
		SP_REACT: typeof React;
		SP_REACTDOM: any;
		SP_JSX_FACTORY: any;
	}
}

/**
 * Create a Regular Expression to search for a React component that uses certain props in order.
 *
 * @param {string[]} propList Ordererd list of properties to search for
 * @returns {RegExp} RegEx to call .test(component.toString()) on
 */
export function createPropListRegex(propList: string[], fromStart: boolean = true): RegExp {
	let regexString = fromStart ? 'const{' : '';
	propList.forEach((prop: any, propIdx) => {
		regexString += `"?${prop}"?:[a-zA-Z_$]{1,2}`;
		if (propIdx < propList.length - 1) {
			regexString += ',';
		}
	});

	// TODO provide a way to enable this
	// console.debug(`[DFL:Utils] createPropListRegex generated regex "${regexString}" for props`, propList);

	return new RegExp(regexString);
}

let oldHooks = {};

function getReactInternals(): any {
	const react = window.SP_REACT as any;
	let internals =
		/** react <18 */
		react?.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED ??
		/** react >18 */
		react?.__CLIENT_INTERNALS_DO_NOT_USE_OR_WARN_USERS_THEY_CANNOT_UPGRADE;

	if (internals?.ReactCurrentDispatcher?.current) {
		return internals.ReactCurrentDispatcher.current;
	}

	return Object.values(internals).find((module: any) => module?.useEffect && module?.useContext && module?.useRef && module?.useState);
}

export function applyHookStubs(customHooks: any = {}): any {
	const hooks = getReactInternals();

	// TODO: add more hooks

	oldHooks = {
		useContext: hooks.useContext,
		useCallback: hooks.useCallback,
		useLayoutEffect: hooks.useLayoutEffect,
		useEffect: hooks.useEffect,
		useMemo: hooks.useMemo,
		useRef: hooks.useRef,
		useState: hooks.useState,
	};

	hooks.useCallback = (cb: Function) => cb;
	hooks.useContext = (cb: any) => cb._currentValue;
	hooks.useLayoutEffect = (_: Function) => {}; //cb();
	hooks.useMemo = (cb: Function, _: any[]) => cb;
	hooks.useEffect = (_: Function) => {}; //cb();
	hooks.useRef = (val: any) => ({ current: val || {} });
	hooks.useState = (v: any) => {
		let val = v;

		return [val, (n: any) => (val = n)];
	};

	Object.assign(hooks, customHooks);

	return hooks;
}

export function removeHookStubs() {
	const hooks = getReactInternals();
	Object.assign(hooks, oldHooks);
	oldHooks = {};
}

export function fakeRenderComponent(fun: Function, customHooks?: any): any {
	const hooks = applyHookStubs(customHooks);

	const res = fun(hooks); // TODO why'd we do this?

	removeHookStubs();

	return res;
}

export function wrapReactType(node: any, prop: any = 'type') {
	if (node[prop]?.__MILLENNIUM_WRAPPED) {
		return node[prop];
	} else {
		return (node[prop] = { ...node[prop], __MILLENNIUM_WRAPPED: true });
	}
}

export function wrapReactClass(node: any, prop: any = 'type') {
	if (node[prop]?.__MILLENNIUM_WRAPPED) {
		return node[prop];
	} else {
		const cls = node[prop];
		const wrappedCls = class extends cls {
			static __MILLENNIUM_WRAPPED = true;
		};
		return (node[prop] = wrappedCls);
	}
}

export function getReactRoot(o: HTMLElement | Element | Node) {
	return (o as any)[Object.keys(o).find((k) => k.startsWith('__reactContainer$')) as string] || (o as any)['_reactRootContainer']?._internalRoot?.current;
}

export function getReactInstance(o: HTMLElement | Element | Node) {
	return (
		(o as any)[Object.keys(o).find((k) => k.startsWith('__reactFiber')) as string] ||
		(o as any)[Object.keys(o).find((k) => k.startsWith('__reactInternalInstance')) as string]
	);
}

// Based on https://github.com/GooseMod/GooseMod/blob/9ef146515a9e59ed4e25665ed365fd72fc0dcf23/src/util/react.js#L20
export interface findInTreeOpts {
	walkable?: string[];
	ignore?: string[];
}

export declare type findInTreeFilter = (element: any) => boolean;

export const findInTree = (parent: any, filter: findInTreeFilter, opts: findInTreeOpts): any => {
	const { walkable = null, ignore = [] } = opts ?? {};

	if (!parent || typeof parent !== 'object') {
		// Parent is invalid to search through
		return null;
	}

	if (filter(parent)) return parent; // Parent matches, just return

	if (Array.isArray(parent)) {
		// Parent is an array, go through values
		return parent.map((x) => findInTree(x, filter, opts)).find((x) => x);
	}

	// Parent is an object, go through values (or option to only use certain keys)
	return (walkable || Object.keys(parent)).map((x) => !ignore.includes(x) && findInTree(parent[x], filter, opts)).find((x: any) => x);
};

export const findInReactTree = (node: any, filter: findInTreeFilter) =>
	findInTree(node, filter, {
		// Specialised findInTree for React nodes
		walkable: ['props', 'children', 'child', 'sibling'],
	});

/**
 * Finds the parent window of a DOM element
 */
export function getParentWindow<WindowType = Window>(elem: HTMLElement | null): WindowType | null | undefined {
	return elem?.ownerDocument?.defaultView as any;
}

/**
 * React hook to find the host window of a component
 * Pass the returned ref into a React element and window will be its host window.
 * @returns [ref, window]
 */
export function useWindowRef<RefElementType extends HTMLElement, WindowType = Window>(): [Ref<RefElementType>, WindowType | null | undefined] {
	const [win, setWin] = useState<WindowType | null | undefined>(null);

	return [(elem) => setWin(getParentWindow<WindowType>(elem)), win];
}
