declare global {
	interface Window {
		MILLENNIUM_API: any;
		SP_REACTDOM: any;
		MILLENNIUM_IPC_PORT: number;
		MILLENNIUM_FRONTEND_LIB_VERSION: string;
		MILLENNIUM_BROWSER_LIB_VERSION: string;
		MILLENNIUM_LOADER_BUILD_DATE: string;
	}
}

import { Logger } from '@steambrew/client/build/logger';

class Bootstrap {
	logger: Logger;
	millenniumAuthToken: string | undefined = undefined;

    init(authToken: string) {
		this.millenniumAuthToken = authToken;

		window.MILLENNIUM_FRONTEND_LIB_VERSION = process.env.MILLENNIUM_FRONTEND_LIB_VERSION || 'unknown';
		window.MILLENNIUM_BROWSER_LIB_VERSION = process.env.MILLENNIUM_FRONTEND_LIB_VERSION || 'unknown';
		window.MILLENNIUM_LOADER_BUILD_DATE = process.env.MILLENNIUM_LOADER_BUILD_DATE || 'unknown';

        this.logger = new Logger('SDK');
		this.logger.log('Loading Millennium Software Development Kit...');
	}

	async injectLegacyReactGlobals() {
		if (!window.SP_REACT) {
			const webpack = await import('@steambrew/client/build/webpack');

			window.SP_REACT = webpack.findModule((m) => m.Component && m.PureComponent && m.useLayoutEffect);
			window.SP_REACTDOM =
				/** react 18 react dom */
				webpack.findModule((m) => m.createPortal && m.createRoot) /** react 19 react dom */ || {
					...webpack.findModule((m) => m.createPortal && m.__DOM_INTERNALS_DO_NOT_USE_OR_WARN_USERS_THEY_CANNOT_UPGRADE),
					...webpack.findModule((m) => m.createRoot),
				};

			/* < mar 19 2026 */
			const oldJsx = webpack.findModule((m) => m.jsx && Object.keys(m).length == 1)?.jsx;
			/* >= mar 19 2026*/
			const newJsx = webpack.findModule((m) => m?.jsx && m?.Fragment && m?.jsxs);

			if (oldJsx) {
				window.SP_JSX_FACTORY = {
					Fragment: window.SP_REACT.Fragment,
					jsx: oldJsx,
					jsxs: oldJsx,
				};
			} else if (newJsx) {
				window.SP_JSX_FACTORY = newJsx;
			} else {
				this.logger.error('Failed to find JSX Factory!');
			}
		}
		const steambrewClientModule = await import('@steambrew/client');
		const millenniumApiModule = await import('./millennium-api');

		/** Set Auth Token */
		millenniumApiModule.setMillenniumAuthToken(this.millenniumAuthToken);
		Object.assign((window.MILLENNIUM_API ??= {}), steambrewClientModule, millenniumApiModule);
	}

	waitForClientReady(): Promise<void> {
		const checkReady = async (resolve: () => void, interval) => {
			// @ts-expect-error Part of the builtin Steam Client API.
			if (!window.App?.BFinishedInitStageOne()) return;
			clearInterval(interval);
			await this.injectLegacyReactGlobals();
			resolve();
		};

		return new Promise((resolve) => {
			const interval = setInterval(() => checkReady(resolve, interval), 0);
		});
	}

	private appendScriptTag(src: string): HTMLScriptElement | null {
		if (document.querySelector(`script[src="${src}"][type="module"]`)) return null;
		const script = Object.assign(document.createElement('script'), {
			src,
			type: 'module',
			id: 'millennium-injected',
		});
		document.head.appendChild(script);
		return script;
	}

	async appendShimsToDOM(shimList: string[]) {
		/** Inject the JavaScript shims into the DOM */
		shimList?.forEach((shim) => this.appendScriptTag(shim));
	}

	async importShimsInContext(shimList: string[]) {
		/** Import the JavaScript shims in the current context */
		await Promise.all(shimList?.map((shim) => import(shim)) ?? []);
	}

	async startWebkitPreloader(millenniumAuthToken: string, enabledPlugins?: string[], legacyShimList?: string[], ctxShimList?: string[], ftpBasePath?: string) {
		this.init(millenniumAuthToken);
		const millenniumApiModule = await import('./millennium-api');

		millenniumApiModule.setMillenniumAuthToken(this.millenniumAuthToken);
		window.MILLENNIUM_API = millenniumApiModule;

		const browserUtils = await import('./browser-init');
		await browserUtils.appendAccentColor();
		await browserUtils.appendQuickCss();
		await browserUtils.addPluginDOMBreadCrumbs(enabledPlugins);

		/** Inject the JavaScript shims into the DOM */
		await this.appendShimsToDOM(legacyShimList);

		/** Import the JavaScript shims in the current context */
		await this.importShimsInContext(ctxShimList);
	}

	async StartPreloader(millenniumAuthToken: string, shimList?: string[]) {
		this.init(millenniumAuthToken);

		await this.waitForClientReady();

		if (shimList?.length) {
			this.appendShimsToDOM(shimList);
		}
	}
}

export default Bootstrap;
