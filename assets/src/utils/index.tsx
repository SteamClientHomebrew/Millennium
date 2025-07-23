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

import { callable, ConfirmModal, ConfirmModalProps, pluginSelf, showModal } from '@steambrew/client';
import { PluginComponent } from '../types';
import { Logger } from './Logger';
import { SteamLocale } from '../../locales';

export namespace Utils {
	export function BrowseLocalFolder(path: string) {
		SteamClient.System.OpenLocalDirectoryInSystemExplorer(path);
	}

	export function OpenUrl(url: string) {
		Logger.Log('Opening URL:', url);
		SteamClient.System.OpenInSystemBrowser(url);
	}

	export async function GetPluginAssetUrl(): Promise<string> {
		const FindAllPlugins = callable<[], string>('find_all_plugins');

		const plugins: Array<PluginComponent> = JSON.parse(await FindAllPlugins());
		const injectedScript = Array.from(document.scripts).find((script) => script.id === 'millennium-injected');

		if (!injectedScript || !injectedScript.src) {
			return String();
		}

		const url = new URL(injectedScript.src);
		const pluginPath = plugins.find((plugin) => plugin?.data?.name === 'core').path.replace(/\\/g, '/');

		const encoded = pluginPath
			.split('/')
			.map((part) => encodeURIComponent(part))
			.join('/');

		return `https://${url.hostname}:${url.port}/${encoded}/.millennium/Dist`;
	}

	export const PromptReload = (callback: (hasClickedOk: boolean) => void) => {
		const title = SteamLocale('#Settings_RestartRequired_Title');
		const description = SteamLocale('#Settings_RestartRequired_Description');
		const buttonText = SteamLocale('#Settings_RestartNow_ButtonText');

		showModal(
			<ConfirmModal strTitle={title} strDescription={description} strOKButtonText={buttonText} onOK={callback.spread(true)} onCancel={callback.spread(false)} />,
			pluginSelf.mainWindow,
			{
				bNeverPopOut: false,
			},
		);
	};

	export const URLComponent = ({ url }: { url: string }) => {
		return (
			<a href="#" onClick={Utils.OpenUrl.spread(url)}>
				{url}
			</a>
		);
	};

	/**
	 *
	 * @param dateString - The date string to convert to a relative time format.
	 * @returns
	 */
	export const toTimeAgo = (dateString: string) => {
		const rtf = new Intl.RelativeTimeFormat(navigator.language, { numeric: 'auto' });
		const seconds = Math.floor((Date.now() - new Date(dateString).getTime()) / 1000);
		const intervals = { year: 31536000, month: 2592000, week: 604800, day: 86400, hour: 3600, minute: 60, second: 1 };

		for (const unit in intervals) {
			const value = intervals[unit as keyof typeof intervals];
			if (seconds >= value) return rtf.format(-Math.floor(seconds / value), unit as Intl.RelativeTimeFormatUnit);
		}

		return rtf.format(0, 'second');
	};

	export const ShowMessageBox = (message: string, title?: string, props?: ConfirmModalProps) => {
		return new Promise((resolve) => {
			const onOK = () => resolve(true);
			const onCancel = () => resolve(false);

			showModal(
				<ConfirmModal strTitle={title ?? SteamLocale('#InfoSettings_Title')} strDescription={message} onOK={onOK} onCancel={onCancel} {...props} />,
				pluginSelf.mainWindow,
				{
					bNeverPopOut: false,
				},
			);
		});
	};
}

type Drop<N extends number, T extends any[], I extends any[] = []> = I['length'] extends N ? T : T extends [any, ...infer R] ? Drop<N, R, [any, ...I]> : [];

declare global {
	interface Function {
		spread<Args extends any[], R, Bound extends Partial<Args>>(this: (...args: Args) => R, ...boundArgs: Bound): (...args: Drop<Bound['length'], Args>) => R;
	}
}

Function.prototype.spread = function (...boundArgs: any[]) {
	return this.bind(null, ...boundArgs);
};
