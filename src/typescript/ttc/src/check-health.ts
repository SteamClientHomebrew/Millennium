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

import path from 'path';
import { existsSync, readFile } from 'fs';
import { Logger } from './logger';
import { PluginJson } from './plugin-json';

export const ValidatePlugin = (bIsMillennium: boolean, target: string): Promise<PluginJson> => {
	return new Promise<PluginJson>((resolve, reject) => {
		if (!existsSync(target)) {
			Logger.error(`target path does not exist: ${target}`);
			reject();
			return;
		}

		if (bIsMillennium) {
			resolve({
				name: 'core',
				common_name: 'Millennium',
				description: 'An integrated plugin that provides core platform functionality.',
				useBackend: false,
				frontend: '.',
			});
			return;
		}

		const pluginModule = path.join(target, 'plugin.json');

		if (!existsSync(pluginModule)) {
			Logger.error(`plugin.json not found: ${pluginModule}`);
			reject();
			return;
		}

		readFile(pluginModule, 'utf8', (err, data) => {
			if (err) {
				Logger.error(`could not read plugin.json: ${pluginModule}`);
				reject();
				return;
			}

			try {
				const parsed = JSON.parse(data);
				if (!('name' in parsed)) {
					Logger.error('plugin.json is missing required "name" field');
					reject();
				} else {
					resolve(parsed);
				}
			} catch (parseError) {
				Logger.error('plugin.json contains invalid JSON:', pluginModule);
				reject();
			}
		});
	});
};
