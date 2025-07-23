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

import english from './locales/english.json';
import polish from './locales/polish.json';
import spanish from './locales/spanish.json';
import indonesian from './locales/indonesian.json';
import schinese from './locales/schinese.json';
import german from './locales/german.json';
import russian from './locales/russian.json';
import italian from './locales/italian.json';
import swedish from './locales/swedish.json';
import vietnamese from './locales/vietnamese.json';
import brazilian from './locales/brazilian.json';
import french from './locales/french.json';
import turkish from './locales/turkish.json';
import { Logger } from '../src/utils/Logger';

interface LocalizationData {
	[key: string]: string;
}

const handler: ProxyHandler<any> = {
	get: function (target, property: keyof any) {
		if (property in target) {
			return target[property];
		} else {
			try {
				// fallback to english if the target string wasn't found
				return (english as any)?.[property];
			} catch (exception) {
				return 'unknown translation key';
			}
		}
	},
};

export let locale: LocalizationData = new Proxy(english, handler);

const localizationFiles: { [key: string]: LocalizationData } = {
	english,
	polish,
	spanish,
	indonesian,
	schinese,
	german,
	russian,
	italian,
	vietnamese,
	swedish,
	brazilian,
	french,
	turkish,
	// Add other languages here
};

const GetLocalization = async () => {
	const language = await SteamClient.Settings.GetCurrentLanguage();
	Logger.Log(`loading ${language} locales`, localizationFiles?.[language]);

	if (localizationFiles.hasOwnProperty(language)) {
		locale = new Proxy(localizationFiles[language], handler);
	} else {
		Logger.Warn(`Localization for language ${language} not found, defaulting to English.`);
	}
};

// setup locales on startup
GetLocalization();

interface FormatString {
	(template: string, ...args: string[]): string;
}

// @ts-ignore
export const SteamLocale = (strLocale: string) => LocalizationManager.LocalizeString(strLocale);

const formatString: FormatString = (template, ...args) => {
	return template.replace(/{(\d+)}/g, (match, index) => {
		return index < args.length ? args[index] : match; // Replace {index} with the corresponding argument or leave it unchanged
	});
};

export { formatString };
