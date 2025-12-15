enum Context {
	Client,
	Browser,
}

declare global {
	interface Window {
		MILLENNIUM_API: object;
		SP_REACTDOM: any;
		MILLENNIUM_IPC_PORT: number;
		MILLENNIUM_FRONTEND_LIB_VERSION: string;
		MILLENNIUM_BROWSER_LIB_VERSION: string;
		MILLENNIUM_LOADER_BUILD_DATE: string;
	}
}

class Bootstrap {
	logger: import('@steambrew/client/build/logger').default;
	startTime: number;
	ctx: Context;
	millenniumAuthToken: string | undefined = undefined;

	async init() {
		const loggerModule = await import('@steambrew/client/build/logger');

		this.logger = new loggerModule.default('Bootstrap');
		this.ctx = window.location.hostname === 'steamloopback.host' ? Context.Client : Context.Browser;
		this.startTime = performance.now();
	}

	async injectLegacyReactGlobals() {
		this.logger.log('Injecting Millennium API...');

		if (!window.SP_REACT) {
			this.logger.log('Injecting legacy React globals...');

			const webpack = await import('@steambrew/client/build/webpack');

			window.SP_REACT = webpack.findModule((m) => m.Component && m.PureComponent && m.useLayoutEffect);
			window.SP_REACTDOM =
				/** react 18 react dom */
				webpack.findModule((m) => m.createPortal && m.createRoot) ||
					/** react 19 react dom */
					{
						...webpack.findModule((m) => m.createPortal && m.__DOM_INTERNALS_DO_NOT_USE_OR_WARN_USERS_THEY_CANNOT_UPGRADE),
						...webpack.findModule((m) => m.createRoot),
					};

			const jsx = webpack.findModule((m) => m.jsx && Object.keys(m).length == 1)?.jsx;

			if (jsx) {
				window.SP_JSX_FACTORY = {
					Fragment: window.SP_REACT.Fragment,
					jsx,
					jsxs: jsx,
				};
			}
		}

		this.logger.log('Injecting Millennium frontend library...');

		const steambrewClientModule = await import('@steambrew/client');
		const millenniumApiModule = await import('./millennium-api');

		/** Set Auth Token */
		millenniumApiModule.setMillenniumAuthToken(this.millenniumAuthToken);

		Object.assign((window.MILLENNIUM_API ??= {}), steambrewClientModule, millenniumApiModule);
		this.logger.log('Millennium API injected successfully.', window.MILLENNIUM_API);
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

	async StartPreloader(millenniumAuthToken: string, shimList?: string[], enabledPlugins?: string[]) {
		await this.init();
		this.millenniumAuthToken = millenniumAuthToken;

		window.MILLENNIUM_FRONTEND_LIB_VERSION = process.env.MILLENNIUM_FRONTEND_LIB_VERSION || 'unknown';
		window.MILLENNIUM_BROWSER_LIB_VERSION = process.env.MILLENNIUM_FRONTEND_LIB_VERSION || 'unknown';
		window.MILLENNIUM_LOADER_BUILD_DATE = process.env.MILLENNIUM_LOADER_BUILD_DATE || 'unknown';

		switch (this.ctx) {
			case Context.Client: {
				this.logger.log('Running in client context...');
				await this.waitForClientReady();
				break;
			}
			case Context.Browser: {
				this.logger.log('Running in browser context...');
				const millenniumApiModule = await import('./millennium-api');
				millenniumApiModule.setMillenniumAuthToken(this.millenniumAuthToken);

				window.MILLENNIUM_API = millenniumApiModule;

				const browserUtils = await import('./browser-init');
				await browserUtils.appendAccentColor();
				await browserUtils.appendQuickCss();

				await browserUtils.addPluginDOMBreadCrumbs(enabledPlugins);
				break;
			}
			default: {
				this.logger.error("Unknown context, can't load Millennium:", this.ctx);
				return;
			}
		}

		this.logger.log('Loading user plugins...');

		/** Inject the JavaScript shims into the DOM */
		shimList?.forEach(
			(shim) =>
				!document.querySelector(`script[src="${shim}"][type="module"]`) &&
				document.head.appendChild(
					Object.assign(document.createElement('script'), {
						src: shim,
						type: 'module',
						id: 'millennium-injected',
					}),
				),
		);

		const endTime = performance.now(); // End timing
		const connectionTime = endTime - this.startTime;
		this.logger.log(`Successfully injected shims into the DOM in ${connectionTime.toFixed(2)} ms.`);
	}
}

export default Bootstrap;
