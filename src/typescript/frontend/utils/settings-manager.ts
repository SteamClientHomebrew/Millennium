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

import { backend } from './ffi';
import { AppConfig } from './AppConfig';

class SettingsManager {
	private settings!: AppConfig;
	private updateFn: ((recipe: (draft: AppConfig) => void) => void) | null = null;
	private pendingUpdates: ((draft: AppConfig) => void)[] = [];

	constructor() {
		backend.config.millennium.getConfig().then((cfg) => {
			this.settings = cfg;
		});
	}

	public getConfig(): AppConfig {
		return this.settings;
	}

	public get config(): AppConfig {
		return this.settings;
	}

	public setInitialConfig(config: AppConfig) {
		this.settings = config;
	}

	public setUpdateFunction(updateFn: (recipe: (draft: AppConfig) => void) => void) {
		this.updateFn = updateFn;
		this.pendingUpdates.forEach((recipe) => this.updateFn!(recipe));
		this.pendingUpdates = [];
	}

	public updateConfig(recipe: (draft: AppConfig) => void) {
		if (this.updateFn) {
			this.updateFn(recipe);
		} else {
			this.pendingUpdates.push(recipe);
		}
	}

	public setConfigDirect(newConfig: AppConfig) {
		this.settings = newConfig;
	}

	/**
	 * Force save the config to the backend.
	 * This is useful when the config is changed, but no config provider is available.
	 * In the case no provider is available, the config will not be saved; so this function is used to force the config to be saved.
	 */
	public forceSaveConfig() {
		backend.config.millennium.setConfig(JSON.stringify(this.settings), true);
	}
}

export const settingsManager = new SettingsManager();
