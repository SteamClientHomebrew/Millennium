import Logger from './logger';

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

function initModuleCache() {
	const startTime = performance.now();
	logger.group('Webpack Module Init');
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

	logger.log('Initializing all modules. Errors here likely do not matter, as they are usually just failing module side effects.');

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

	logger.groupEnd(`Modules initialized in ${performance.now() - startTime}ms...`);
}

initModuleCache();

export const findModule = (filter: FilterFn) => {
	for (const m of modules.values()) {
		if (m.default && filter(m.default)) return m.default;
		if (filter(m)) return m;
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
		if (m.default && filter(m.default)) out.push(m.default);
		if (filter(m)) out.push(m);
	}

	return out;
};

export const createModuleMapping = (filter: FilterFn) => {
	const mapping = new Map<ModuleID, Module>();

	for (const [id, m] of modules) {
		if (m.default && filter(m.default)) mapping.set(id, m.default);
		if (filter(m)) mapping.set(id, m);
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

export const IconsModule = findModuleByExport((e) => e?.toString && /Spinner\)}\)?,.\.createElement\(\"path\",{d:\"M18 /.test(e.toString()));

export const ReactRouter = findModuleByExport((e) => e.computeRootMatch);
