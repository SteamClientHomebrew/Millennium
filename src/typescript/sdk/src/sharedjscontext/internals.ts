export const INTERNAL_EXPORT_PREFIX = '__millennium_internal_';
export const CLASS_MAP_EXPORT_PREFIX = '__millennium_classmap_';

const exportedInternals: Record<string, any> = {};

export function registerExportedInternal(name: string, value: any): void {
	exportedInternals[name] = value;
}

export function nativeInternal<T = any>(name: string): T | undefined {
	return exportedInternals[name];
}
