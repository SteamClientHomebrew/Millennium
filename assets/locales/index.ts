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
