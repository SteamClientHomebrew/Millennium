import type { FC, ReactElement } from 'react';

type JsxKey = 'jsx' | 'jsxs';

const JSX_KEYS: JsxKey[] = ['jsx', 'jsxs'];

export interface ComponentInterceptor {
	unpatch(): void;
}

export type ComponentWrapperFactory = (renderOriginal: (props: any) => ReactElement) => FC<any>;

export function interceptComponent(matchesSource: (source: string) => boolean, createWrapper: ComponentWrapperFactory): ComponentInterceptor {
	const factory = window.SP_JSX_FACTORY;
	const react = window.SP_REACT as any;

	const originalJsx = Object.fromEntries(JSX_KEYS.map((key) => [key, factory[key]])) as Record<JsxKey, (...args: any[]) => any>;
	const originalCreateElement = react.createElement as (...args: any[]) => any;

	const resolved = new WeakMap<Function, Function | null>();

	const resolve = (type: unknown) => {
		if (typeof type !== 'function') return type;

		let wrapper = resolved.get(type);
		if (wrapper === undefined) {
			let source = '';
			try {
				source = Function.prototype.toString.call(type);
			} catch {}

			if (matchesSource(source)) {
				const created = createWrapper((props) => originalCreateElement(type, props));
				created.displayName = (type as FC).displayName || type.name;
				wrapper = created;
			} else {
				wrapper = null;
			}
			resolved.set(type, wrapper);
		}
		return wrapper ?? type;
	};

	const patchedJsx = {} as Record<JsxKey, (...args: any[]) => any>;
	for (const key of JSX_KEYS) {
		patchedJsx[key] = (type: unknown, ...rest: any[]) => originalJsx[key](resolve(type), ...rest);
		factory[key] = patchedJsx[key];
	}

	const patchedCreateElement = (type: unknown, ...rest: any[]) => originalCreateElement(resolve(type), ...rest);
	react.createElement = patchedCreateElement;

	return {
		unpatch() {
			for (const key of JSX_KEYS) {
				if (factory[key] === patchedJsx[key]) factory[key] = originalJsx[key];
			}
			if (react.createElement === patchedCreateElement) react.createElement = originalCreateElement;
		},
	};
}
