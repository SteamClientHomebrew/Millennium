import { callOriginal, findClassModule, Patch, replacePatch, sleep, ChromeDevToolsProtocol } from '@steambrew/client';

/**
 * BrowserManagerHook is a utility class that hooks into the MainWindowBrowserManager
 * to hide it when the MillenniumSidebar is open.
 *
 * The main browser window is a child HWND of the main steam window. It's directly rendered on top of the main steam window, they are separate windows, not one in the same.
 * This means if the MillenniumSidebar is open, it would be covering the browser window, which is not ideal.
 *
 * This is a decent solution that hides the browser, then screenshots it and display it as a background when the sidebar is open.
 */
export class BrowserManagerHook {
	private setBrowserVisiblePatched?: Patch;
	private setBrowserBoundsPatched?: Patch;

	private shouldBlockRequest = false;
	private inputBlockerClass = findClassModule((m) => m.BrowserWrapper && m.Browser) || {};

	private async captureBrowserSnapshot() {
		const targetInfos: any = await ChromeDevToolsProtocol.send('Target.getTargets');

		const browserTarget = targetInfos?.targetInfos?.find((target: any) => target?.url === MainWindowBrowserManager.m_URL);

		const attachedTarget: any = await ChromeDevToolsProtocol.send('Target.attachToTarget', {
			targetId: browserTarget?.targetId,
			flatten: true,
		});

		const screenshot: any = await ChromeDevToolsProtocol.send(
			'Page.captureScreenshot',
			{
				format: 'jpeg',
				quality: 75,
			},
			attachedTarget?.sessionId,
		);

		return `data:image/jpeg;base64,${screenshot?.data}`;
	}

	public async setBrowserVisible(windowRef: Window, visible: boolean) {
		if (!MainWindowBrowserManager?.m_lastLocation?.pathname.startsWith('/browser/')) {
			return;
		}

		const inputBlocker = windowRef?.document?.getElementsByClassName(this.inputBlockerClass.Browser)[0] as HTMLElement | null;

		if (inputBlocker && !visible) {
			const snapShot = await this.captureBrowserSnapshot();
			inputBlocker.style.setProperty('-valve-app-region', visible ? '' : 'unset');
			inputBlocker.style.background = `url(${snapShot}) no-repeat center center`;
			inputBlocker.style.backgroundSize = '100% auto';
		}

		/** Give the snapshot image some time to load in to prevent flickering */
		!visible && (await sleep(50));

		if (typeof MainWindowBrowserManager !== 'undefined') {
			if (this.setBrowserVisiblePatched?.original && typeof this.setBrowserBoundsPatched?.original === 'function') {
				this.setBrowserVisiblePatched.original(visible);
			} else {
				console.warn('Not hooked yet, falling back to original SetVisible');
				MainWindowBrowserManager?.m_browser?.SetVisible(visible);
			}
		}

		// Remove the pseudo background when dismounting
		if (inputBlocker && visible) {
			await sleep(50); // No rush to remove the background its not visible anyway
			inputBlocker.style.background = '';
		}
	}

	private setBoundsHookCb() {
		console.warn('MainWindowBrowserManager.SetVisible called, blocking request', this.shouldBlockRequest);

		return this.shouldBlockRequest ? null : callOriginal;
	}

	private setVisibleHookCb() {
		console.warn('MainWindowBrowserManager.SetVisible called, blocking request', this.shouldBlockRequest);

		return this.shouldBlockRequest ? null : callOriginal;
	}

	public async hook(skipCheckHealth: boolean = false) {
		while (
			typeof MainWindowBrowserManager === 'undefined' ||
			MainWindowBrowserManager?.m_browser?.SetBounds === undefined ||
			MainWindowBrowserManager?.m_browser?.SetVisible === undefined
		) {
			await sleep(10);
		}

		this.setBrowserBoundsPatched = replacePatch(MainWindowBrowserManager.m_browser, 'SetBounds', this.setBoundsHookCb.bind(this));
		this.setBrowserVisiblePatched = replacePatch(MainWindowBrowserManager.m_browser, 'SetVisible', this.setVisibleHookCb.bind(this));

		if (!skipCheckHealth) {
			this.hookHealthCheck();
		}
	}

	public async unhook() {
		this.setBrowserBoundsPatched?.unpatch();
		this.setBrowserVisiblePatched?.unpatch();
	}

	public setShouldBlockRequest(block: boolean) {
		this.shouldBlockRequest = block;
	}

	/** It seems the SetBounds and SetVisible are re-instantiated sometimes, which overrides our patch */
	private hookHealthCheck() {
		const interval = setInterval(() => {
			const browser = MainWindowBrowserManager?.m_browser;

			if (browser?.SetBounds !== this.setBrowserBoundsPatched.patchedFunction || browser?.SetVisible !== this.setBrowserVisiblePatched.patchedFunction) {
				console.warn('MainWindowBrowserManager changed, re-hooking...');
				this.hook(true);
			}
		}, 100);

		setTimeout(() => clearInterval(interval), 10000);
	}
}
