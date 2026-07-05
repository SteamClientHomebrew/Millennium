import { Logger } from './logger';

declare global {
	interface Window {
		webpackChunksteamui: any;
	}
}

const logger = new Logger('Webpack');

// In most case an object with getters for each property. Look for the first call to r.d in the module, usually near or at the top.
export type ModuleID = string; // number string
export type Module = any;
export type Export = any;
type FilterFn = (module: any) => boolean;
type ExportFilterFn = (moduleExport: any, exportName?: any) => boolean;
type FindFn = (module: any) => any;

export let modules = new Map<ModuleID, Module>();

export function isCssHash(hash: string): boolean {
	const len = hash.length;
	if (len < 18 || len > 30) return false;
	if (!/^[A-Za-z0-9_-]+$/.test(hash)) return false;

	const hasDigit = /[0-9]/.test(hash);
	const hasConsecUpper = /[A-Z]{2}/.test(hash);
	const hasUpper = /[A-Z]/.test(hash);
	const hasHyphenMixed = hash.includes('-') && (hasDigit || hasUpper);

	return hasDigit || hasConsecUpper || hasHyphenMixed;
}

export function firstClass(value: string): string {
	const parts = value.split(' ');
	return parts.length === 2 ? parts[0] : value;
}

export function isClassModule(m: Module): boolean {
	if (typeof m !== 'object' || m == null || m.__esModule) return false;
	const keys = Object.keys(m);
	if (keys.length === 0) return false;

	let hasHash = false;
	for (const k of keys) {
		if (Object.getOwnPropertyDescriptor(m, k)?.get || typeof m[k] !== 'string') return false;
		if (isCssHash(firstClass(m[k]))) hasHash = true;
	}
	return hasHash;
}

const resolvedModuleCache = new WeakMap<object, Module>();

function resolveModule(m: Module): Module {
	if (typeof m !== 'object' || m == null) return m;

	const cached = resolvedModuleCache.get(m);
	if (cached !== undefined) return cached;

	const resolved = isClassModule(m) ? Object.fromEntries((Object.entries(m) as [string, string][]).map(([key, value]) => [key, firstClass(value)])) : m;
	resolvedModuleCache.set(m, resolved);
	return resolved;
}

function initModuleCache() {
	// Webpack 5, currently on beta
	// Generate a fake module ID
	const id = Symbol('@steambrew/client');
	let webpackRequire!: ((id: any) => Module) & { m: object };
	// Insert our module in a new chunk.
	// The module will then be called with webpack's internal require function as its first argument
	window.webpackChunksteamui.push([
		[id],
		{},
		(r: any) => {
			webpackRequire = r;
		},
	]);

	// Loop over every module ID
	for (let id of Object.keys(webpackRequire.m)) {
		try {
			const module = webpackRequire(id);
			if (module) {
				modules.set(id, module);
			}
		} catch (e) {
			logger.debug('Ignoring require error for module', id, e);
		}
	}
}

initModuleCache();

export const findModule = (filter: FilterFn) => {
	for (const m of modules.values()) {
		if (m.default && filter(m.default)) return resolveModule(m.default);
		if (filter(m)) return resolveModule(m);
	}
};

export const findModuleDetailsByExport = (
	filter: ExportFilterFn,
	minExports?: number,
): [module: Module | undefined, moduleExport: any, exportName: any, moduleID: string | undefined] => {
	for (const [id, m] of modules) {
		if (!m) continue;
		for (const mod of [m.default, m]) {
			if (typeof mod !== 'object') continue;
			if (minExports && Object.keys(mod).length < minExports) continue;
			for (let exportName in mod) {
				if (mod?.[exportName]) {
					try {
						const filterRes = filter(mod[exportName], exportName);
						if (filterRes) {
							return [mod, mod[exportName], exportName, id];
						} else {
							continue;
						}
					} catch {}
				}
			}
		}
	}
	return [undefined, undefined, undefined, undefined];
};

export const findModuleByExport = (filter: ExportFilterFn, minExports?: number) => {
	return findModuleDetailsByExport(filter, minExports)?.[0];
};

export const findModuleExport = (filter: ExportFilterFn, minExports?: number) => {
	return findModuleDetailsByExport(filter, minExports)?.[1];
};

/**
 * @deprecated use findModuleExport instead
 */
export const findModuleChild = (filter: FindFn) => {
	for (const m of modules.values()) {
		for (const mod of [m.default, m]) {
			const filterRes = filter(mod);
			if (filterRes) {
				return filterRes;
			} else {
				continue;
			}
		}
	}
};

/**
 * @deprecated use createModuleMapping instead
 */
export const findAllModules = (filter: FilterFn) => {
	const out = [];

	for (const m of modules.values()) {
		if (m.default && filter(m.default)) out.push(resolveModule(m.default));
		if (filter(m)) out.push(resolveModule(m));
	}

	return out;
};

export const createModuleMapping = (filter: FilterFn) => {
	const mapping = new Map<ModuleID, Module>();

	for (const [id, m] of modules) {
		if (m.default && filter(m.default)) mapping.set(id, resolveModule(m.default));
		if (filter(m)) mapping.set(id, resolveModule(m));
	}

	return mapping;
};

export const CommonUIModule = findModule((m: Module) => {
	if (typeof m !== 'object') return false;
	for (let prop in m) {
		if (m[prop]?.contextType?._currentValue && Object.keys(m).length > 60) return true;
	}
	return false;
});

export const ReactRouter = findModuleByExport((e) => e.computeRootMatch);
