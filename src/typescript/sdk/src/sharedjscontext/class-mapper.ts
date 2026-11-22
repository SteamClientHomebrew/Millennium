import { Module, findAllModules, isClassModule, millenniumClassMaps, registerVirtualModules } from './module-registry';
import { CLASS_MAP_MODULES } from './class-map-data';
import { Logger } from './logger';

export interface ClassModule {
	[name: string]: string;
}

interface TableModule {
	names: Set<string>;
	values: ClassModule;
}

const logger = new Logger('ClassMapper');

const MIN_TABLE_PRESENCE = 0.5;
const MIN_LEGACY_MODULES = 20;
const MIN_EXPORTED_MAP_NAMES = 3;

function scanLiveClasses(): Set<string> {
	const found = new Set<string>();
	const pattern = /\.(-?[_a-zA-Z][\w-]*)/g;

	const visit = (rules: CSSRuleList) => {
		for (const rule of Array.from(rules)) {
			const selector = (rule as CSSStyleRule).selectorText;
			if (selector) for (const match of selector.matchAll(pattern)) found.add(match[1]);
			const nested = (rule as CSSGroupingRule).cssRules;
			if (nested) visit(nested);
		}
	};

	for (const sheet of Array.from(document.styleSheets)) {
		try {
			visit(sheet.cssRules);
		} catch {}
	}
	return found;
}

function createExportedModules(liveClasses: Set<string>): TableModule[] {
	const modules: TableModule[] = [];
	for (const map of millenniumClassMaps) {
		const values: ClassModule = {};
		for (const [name, hash] of Object.entries(map)) {
			if (typeof hash === 'string' && liveClasses.has(hash.split(' ')[0])) values[name] = hash.split(' ')[0];
		}
		const names = Object.keys(values);
		if (names.length >= MIN_EXPORTED_MAP_NAMES) modules.push({ names: new Set(names), values });
	}
	return modules;
}

function createTable(liveClasses: Set<string>): TableModule[] {
	let total = 0;
	let present = 0;
	for (const { v } of CLASS_MAP_MODULES) {
		for (const hash of Object.values(v)) {
			total++;
			if (liveClasses.has(hash)) present++;
		}
	}

	if (total === 0 || present / total < MIN_TABLE_PRESENCE) {
		logger.warn(`Bundled class map does not match this Steam build (${present}/${total} classes found), class names will fall back to literal names.`);
		return [];
	}

	return CLASS_MAP_MODULES.map(({ n, v }) => {
		const names = n.split(',');
		const values: ClassModule = {};
		for (const name of names) {
			const resolved = liveClasses.has(name) ? name : v[name];
			if (resolved) values[name] = resolved;
		}
		return { names: new Set(names), values };
	});
}

const legacyCandidates: ClassModule[] = findAllModules((m: Module) => {
	if (typeof m !== 'object' || m == null) return false;

	const keys = Object.keys(m);
	if (keys.length == 1 && m.version) return false;
	if (keys.length > 1000 && m.AboutSettings) return false;

	return isClassModule(m);
});

const legacyClassMapList: ClassModule[] = legacyCandidates.length >= MIN_LEGACY_MODULES ? legacyCandidates : [];

const liveClasses = legacyClassMapList.length === 0 ? scanLiveClasses() : undefined;
const table: TableModule[] = liveClasses ? [...createExportedModules(liveClasses), ...createTable(liveClasses)] : [];

export const classMapList: ClassModule[] = legacyClassMapList.length > 0 ? legacyClassMapList : table.map((m) => m.values);

export const classMap: ClassModule = Object.assign({}, ...classMapList.map((obj) => Object.fromEntries(Object.entries(obj).map(([key, value]) => [key, value]))));

function presenceView(module: TableModule): ClassModule {
	return new Proxy(module.values, {
		get: (target, prop) => (typeof prop === 'string' ? (target[prop] ?? (module.names.has(prop) ? true : undefined)) : undefined),
		has: (_, prop) => typeof prop === 'string' && module.names.has(prop),
		ownKeys: () => [...module.names],
		getOwnPropertyDescriptor: (target, prop) =>
			typeof prop === 'string' && module.names.has(prop) ? { configurable: true, enumerable: true, value: target[prop] } : undefined,
	}) as ClassModule;
}

if (!legacyClassMapList.length) {
	const virtualModules = table.map((module) => ({ view: presenceView(module), values: module.values }));
	registerVirtualModules(() => virtualModules);
}

export function findClass(name: string): string | void {
	if (liveClasses?.has(name)) return name;
	return classMapList.find((m) => m?.[name])?.[name];
}

const findClassHandler = {
	get: (target: any, prop: string) => {
		if (typeof prop === 'string') {
			return target(prop);
		}
	},
};

export const Classes = new Proxy(findClass, findClassHandler);

export function findClassModule(filter: (module: any) => boolean): ClassModule | void {
	if (legacyClassMapList.length > 0) {
		return legacyClassMapList.find((m) => filter(m));
	}

	return table.find((module) => filter(presenceView(module)))?.values;
}

export function unminifyClass(minifiedClass: string): string | void {
	for (let m of classMapList) {
		for (let className of Object.keys(m)) {
			if (m[className] == minifiedClass) return className;
		}
	}
}
