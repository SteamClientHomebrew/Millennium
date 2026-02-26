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
