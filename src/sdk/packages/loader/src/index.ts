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

class Bootstrap {
	logger: import('@steambrew/client/build/logger').default;
	startTime: number;
	millenniumAuthToken: string | undefined = undefined;

	async init(authToken: string) {
		const loggerModule = await import('@steambrew/client/build/logger');
		this.millenniumAuthToken = authToken;

		window.MILLENNIUM_FRONTEND_LIB_VERSION = process.env.MILLENNIUM_FRONTEND_LIB_VERSION || 'unknown';
		window.MILLENNIUM_BROWSER_LIB_VERSION = process.env.MILLENNIUM_FRONTEND_LIB_VERSION || 'unknown';
		window.MILLENNIUM_LOADER_BUILD_DATE = process.env.MILLENNIUM_LOADER_BUILD_DATE || 'unknown';

		this.logger = new loggerModule.default('Bootstrap');
		this.startTime = performance.now();
	}

	async logDbgInfo() {
		const endTime = performance.now();
		const connectionTime = endTime - this.startTime;
		this.logger.log(`done. ${connectionTime.toFixed(2)} ms.`);
	}

	async injectLegacyReactGlobals() {
		this.logger.log('Injecting Millennium API...');

		if (!window.SP_REACT) {
			this.logger.log('Injecting legacy React globals...');

			const webpack = await import('@steambrew/client/build/webpack');

			window.SP_REACT = webpack.findModule((m) => m.Component && m.PureComponent && m.useLayoutEffect);
			window.SP_REACTDOM =
				/** react 18 react dom */
				webpack.findModule((m) => m.createPortal && m.createRoot) /** react 19 react dom */ || {
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

	async appendShimsToDOM(shimList: string[]) {
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
	}

	async importShimsInContext(shimList: string[]) {
		/** Import the JavaScript shims in the current context */
		await Promise.all(shimList?.map((shim) => import(shim)) ?? []);
	}

	/**
	 * Append Millennium API to the public context (web page context)
	 * Millennium is running this script from an isolated context, so to expose the API to the web page,
	 * we need to inject a script tag that runs in the public context.
	 */
	async appendMillenniumToPublicContext(ftpBasePath: string) {
		/** This unfortunately is a dog shit that we have to hardcode the filepath, but there is genuinely no other way to do this currently */
		const code = `(window.MILLENNIUM_API = await import('${ftpBasePath}/chunks/chunk-millennium-api.js')).setMillenniumAuthToken(${JSON.stringify(
			this.millenniumAuthToken,
		)})`;

		const script = document.createElement('script');
		script.type = 'module';
		script.textContent = code;
		document.documentElement.appendChild(script);
	}

	async startWebkitPreloader(millenniumAuthToken: string, enabledPlugins?: string[], legacyShimList?: string[], ctxShimList?: string[], ftpBasePath?: string) {
		await this.init(millenniumAuthToken);

		this.logger.log('bootstrapping plugin webkits...');
		const millenniumApiModule = await import('./millennium-api');

		millenniumApiModule.setMillenniumAuthToken(this.millenniumAuthToken);
		window.MILLENNIUM_API = millenniumApiModule;

		this.appendMillenniumToPublicContext(ftpBasePath);

		const browserUtils = await import('./browser-init');
		await browserUtils.appendRootColors();
		await browserUtils.appendAccentColor();
		await browserUtils.appendQuickCss();
		await browserUtils.addPluginDOMBreadCrumbs(enabledPlugins);

		/** Inject the JavaScript shims into the DOM */
		await this.appendShimsToDOM(legacyShimList);

		/** Import the JavaScript shims in the current context */
		await this.importShimsInContext(ctxShimList);
		this.logDbgInfo();
	}

	async StartPreloader(millenniumAuthToken: string, shimList?: string[], enabledPlugins?: string[]) {
		await this.init(millenniumAuthToken);

		this.logger.log('Running in client context...');
		await this.waitForClientReady();
		this.logger.log('Client is ready.');

		await this.appendShimsToDOM(shimList);
		this.logDbgInfo();
	}
}

export default Bootstrap;
