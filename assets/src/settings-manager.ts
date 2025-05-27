import { produce } from 'immer';
import { PyGetBackendConfig, PySetBackendConfig } from './utils/ffi';
import { AppConfig } from './AppConfig';

class SettingsManager {
	private settings: AppConfig;
	private updateFn: ((recipe: (draft: AppConfig) => void) => void) | null = null;
	private pendingUpdates: ((draft: AppConfig) => void)[] = [];

	constructor() {
		PyGetBackendConfig().then((cfg) => {
			this.settings = JSON.parse(cfg) as AppConfig;
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
			this.updateFn((draft) => {
				recipe(draft);
				this.settings = draft as AppConfig; // Update internal reference
			});
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
		PySetBackendConfig({ config: JSON.stringify(this.settings), skip_propagation: true });
	}
}

export const settingsManager = new SettingsManager();
