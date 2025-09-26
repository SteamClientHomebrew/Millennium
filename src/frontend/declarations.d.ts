/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
