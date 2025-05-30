declare module '*.gif' {
	const value: string;
	export default value;
}

interface Window {
	MILLENNIUM_BROWSER_LIB_VERSION?: string;
	MILLENNIUM_FRONTEND_LIB_VERSION?: string;
	MILLENNIUM_LOADER_BUILD_DATE?: string;
}
