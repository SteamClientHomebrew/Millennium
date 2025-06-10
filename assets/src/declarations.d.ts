declare module '*.gif' {
	const value: string;
	export default value;
}

interface Window {
	MILLENNIUM_BROWSER_LIB_VERSION?: string;
	MILLENNIUM_FRONTEND_LIB_VERSION?: string;
	MILLENNIUM_LOADER_BUILD_DATE?: string;
	MILLENNIUM_SIDEBAR_NAVIGATION_PANELS?: any;
}

declare var MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL: {
	send: (message: string) => void;
	onmessage: (message: string) => void;
};

declare var MainWindowBrowserManager: {
	m_URL?: string;
	m_browser?: {
		/** Hide the main browser HWND */
		SetVisible: (visible: boolean) => void;
		/** Set the bounds of the main browser */
		SetBounds: (x: number, y: number, width: number, height: number) => void;
	};
	m_lastLocation?: {
		pathname: string;
	};
};
