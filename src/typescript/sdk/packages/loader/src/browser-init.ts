import { Millennium } from './millennium-api';

function waitForDocument(): Promise<Document> {
	return new Promise((resolve) => {
		const check = () => {
			/** in the odd case the document doesn't even exist */
			if (typeof document === 'undefined') {
				setTimeout(check, 0);
				return;
			}
			if (document.readyState !== 'loading') {
				resolve(document);
			} else {
				document.addEventListener('DOMContentLoaded', () => resolve(document), { once: true });
			}
		};
		check();
	});
}

function addStyleSheetFromText(document: Document, innerStyle: string, id?: string) {
	if (document.querySelectorAll(`style[id='${id}']`).length) return;

	const style = Object.assign(document.createElement('style'), { id: id });
	style.textContent = innerStyle;
	document.head.appendChild(style);
}
type SystemColors = Record<string, string>;

export async function appendAccentColor() {
	const systemColors: SystemColors = JSON.parse(await Millennium.callServerMethod('core', 'Core_GetSystemColors'));

	const entries = Object.entries(systemColors)
		.map(([key, value]) => {
			const formattedKey = formatCssVarKey(key);
			return `--SystemAccentColor${formattedKey}: ${value};`;
		})
		.join('\n');

	addStyleSheetFromText(await waitForDocument(), `:root {\n${entries}\n}`, 'SystemAccentColorInject');
}

export async function appendQuickCss() {
	try {
		const quickCss: string = JSON.parse(await Millennium.callServerMethod('core', 'Core_LoadQuickCss'));
		addStyleSheetFromText(await waitForDocument(), quickCss, 'MillenniumQuickCss');
	} catch {}
}

export async function addPluginDOMBreadCrumbs(enabledPlugins: string[] = []) {
	const doc = await waitForDocument();
	doc.documentElement.setAttribute('data-millennium-plugin', enabledPlugins.join(' '));
	doc.documentElement.classList.add('MillenniumWindow_SteamBrowser');
}

function formatCssVarKey(key: string) {
	// Capitalize first letter and convert "Rgb" to "-RGB"
	return key.replace(/Rgb$/, '-RGB').replace(/^./, (c) => c.toUpperCase());
}
