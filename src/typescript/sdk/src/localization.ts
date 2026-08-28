/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

/**
 * Plugin localization.
 *
 * A plugin ships a `locales` folder with one JSON file per language, named after
 * Steam's API language code (english.json, russian.json, ...). The build embeds
 * them, so nothing is fetched at runtime and the strings are known offline.
 *
 * Anyone can also translate a plugin they don't own: a translation plugin calls
 * `contributeLocale()` with the target's name, and the target picks the strings up
 * without knowing anything about it. Declaring the target in the translation
 * plugin's `requires` makes Millennium load it after the target and tell the user
 * when it's missing.
 */

const LOCALE_UPDATED_EVENT = 'millennium-locale-updated';
const FALLBACK_LANGUAGE = 'english';

declare global {
	interface Window {
		/** Strings embedded from each plugin's own `locales` folder at build time. */
		MILLENNIUM_PLUGIN_LOCALES: Record<string, Record<string, Record<string, string>>>;
		/** Strings contributed for other plugins, which take precedence over their own. */
		MILLENNIUM_LOCALE_OVERRIDES: Record<string, Record<string, Record<string, string>>>;
		/** The plugins each plugin declares in its manifest, embedded at build time. */
		MILLENNIUM_PLUGIN_DEPENDENCIES: Record<string, string[]>;
	}
}

window.MILLENNIUM_PLUGIN_LOCALES ??= {};
window.MILLENNIUM_LOCALE_OVERRIDES ??= {};
window.MILLENNIUM_PLUGIN_DEPENDENCIES ??= {};

/** Steam's language for this session. Resolved once, English until it arrives. */
let _language = FALLBACK_LANGUAGE;

const _resolveLanguage = (async () => {
	try {
		const language = await (window as any).SteamClient?.Settings?.GetCurrentLanguage?.();
		if (typeof language === 'string' && language.length) {
			_language = language;
			window.dispatchEvent(new CustomEvent(LOCALE_UPDATED_EVENT, { detail: { language } }));
		}
	} catch {
		/* keep the fallback, a missing language is not worth failing over */
	}
})();

/** Replaces {0}, {1}, ... with the given arguments, leaving unmatched ones alone. */
const format = (template: string, args: string[]): string => template.replace(/{(\d+)}/g, (match, index) => args[index] ?? match);

export interface Localization {
	/** Translate a key, formatting any {0}-style placeholders with the extra arguments. */
	(key: string, ...args: string[]): string;
	/** The language these strings resolve against. */
	readonly language: string;
}

const _lookup = (pluginName: string, key: string): string | undefined => {
	const overrides = window.MILLENNIUM_LOCALE_OVERRIDES[pluginName] ?? {};
	const own = window.MILLENNIUM_PLUGIN_LOCALES[pluginName] ?? {};

	/** a contributed translation wins over the plugin's own strings, and both fall
	    back to English so a partial translation only affects the keys it covers */
	return overrides[_language]?.[key] ?? own[_language]?.[key] ?? overrides[FALLBACK_LANGUAGE]?.[key] ?? own[FALLBACK_LANGUAGE]?.[key];
};

/**
 * @brief Get a plugin's translations outside of React.
 *
 * @example
 * ```typescript
 * const t = getLocalization();
 * console.log(t('greeting', 'world'));
 * ```
 */
export function getLocalization(): Localization;
export function getLocalization(pluginName: string): Localization;
export function getLocalization(pluginName?: string): Localization {
	const name = pluginName ?? '';

	const translate = ((key: string, ...args: string[]) => {
		const value = _lookup(name, key);
		if (value === undefined) return key;
		return args.length ? format(value, args) : value;
	}) as Localization;

	Object.defineProperty(translate, 'language', { get: () => _language });
	return translate;
}

/**
 * @brief Get a plugin's translations in a component, re-rendering when they change.
 *
 * The language arrives asynchronously and translations can be contributed by other
 * plugins at any point, so components read strings through this hook rather than
 * capturing them once.
 *
 * @example
 * ```typescript
 * const t = useLocalization();
 * return <Field label={t('settingsTitle')} />;
 * ```
 */
export function useLocalization(): Localization;
export function useLocalization(pluginName: string): Localization;
export function useLocalization(pluginName?: string): Localization {
	const React = (window as any).SP_REACT;
	const name = pluginName ?? '';
	const [, setRevision] = React.useState(0);

	React.useEffect(() => {
		const onLocaleUpdated = (event: Event) => {
			const target = (event as CustomEvent)?.detail?.plugin;
			if (target === undefined || target === name) setRevision((revision: number) => revision + 1);
		};

		window.addEventListener(LOCALE_UPDATED_EVENT, onLocaleUpdated);
		return () => window.removeEventListener(LOCALE_UPDATED_EVENT, onLocaleUpdated);
	}, [name]);

	return React.useMemo(() => getLocalization(name), [name, _language]);
}

/**
 * @brief Translate another plugin, without that plugin doing anything for it.
 *
 * Only plugins named in this plugin's own "requires" or "dependencies" can be
 * translated, so the manifest says what a plugin reaches into and Millennium can tell
 * the user what a translation is for before they install it.
 *
 * The strings replace the target's own for the given language, key by key. Anything
 * left out keeps whatever the target already had, so a partial translation is fine.
 * If the target isn't installed the strings simply sit unused.
 *
 * @example
 * ```typescript
 * // plugin.json: "dependencies": ["some-plugin"]
 * import russian from '../locales/russian.json';
 * contributeLocale('some-plugin', 'russian', russian);
 * ```
 */
export function contributeLocale(targetPlugin: string, language: string, strings: Record<string, string>): void;
export function contributeLocale(pluginName: string, targetPlugin: string, language: string, strings: Record<string, string>): void;
export function contributeLocale(a: string, b: string, c: string | Record<string, string>, d?: Record<string, string>): void {
	const [caller, targetPlugin, language, strings] = d !== undefined ? [a, b, c as string, d] : ['', a, b, c as Record<string, string>];

	if (!targetPlugin || !language || typeof strings !== 'object' || strings === null) return;

	/** a plugin translates what it declares and nothing else */
	if (caller && !(window.MILLENNIUM_PLUGIN_DEPENDENCIES[caller] ?? []).includes(targetPlugin)) {
		console.error(`[Millennium] '${caller}' tried to translate '${targetPlugin}', which it doesn't declare in its plugin.json.`);
		return;
	}

	const target = (window.MILLENNIUM_LOCALE_OVERRIDES[targetPlugin] ??= {});
	target[language] = { ...target[language], ...strings };

	window.dispatchEvent(new CustomEvent(LOCALE_UPDATED_EVENT, { detail: { plugin: targetPlugin, language } }));
}

/**
 * @brief The languages a plugin can be displayed in, its own and contributed ones.
 */
export function getAvailableLanguages(): string[];
export function getAvailableLanguages(pluginName: string): string[];
export function getAvailableLanguages(pluginName?: string): string[] {
	const name = pluginName ?? '';
	const own = Object.keys(window.MILLENNIUM_PLUGIN_LOCALES[name] ?? {});
	const contributed = Object.keys(window.MILLENNIUM_LOCALE_OVERRIDES[name] ?? {});

	return [...new Set([...own, ...contributed])];
}

/** Resolves once Steam's language is known, for callers that need it up front. */
export const localizationReady: Promise<void> = _resolveLanguage;
