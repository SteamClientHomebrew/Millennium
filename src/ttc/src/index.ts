#!/usr/bin/env node

/**
 * this component serves as:
 * - typescript transpiler
 * - rollup configurator
 */
import { BuildType, ValidateParameters } from './query-parser';
import { CheckForUpdates } from './version-control';
import { ValidatePlugin } from './check-health';
import { PluginJson } from './plugin-json';
import { TranspilerPluginComponent, TranspilerProps } from './transpiler';
import { performance } from 'perf_hooks';

declare global {
	var PerfStartTime: number;
}

const CheckModuleUpdates = async () => {
	return await CheckForUpdates();
};

const StartCompilerModule = () => {
	const parameters = ValidateParameters(process.argv.slice(2));
	const bIsMillennium = parameters.isMillennium || false;
	const bTersePlugin = parameters.type == BuildType.ProdBuild;

	ValidatePlugin(bIsMillennium, parameters.targetPlugin)
		.then((json: PluginJson) => {
			const props: TranspilerProps = {
				minify: bTersePlugin,
				pluginName: json.name,
				watch: parameters.watch || false,
				isMillennium: bIsMillennium,
			};

			TranspilerPluginComponent(json, props);
		})
		.catch(() => {
			process.exit();
		});
};

const Initialize = () => {
	global.PerfStartTime = performance.now();

	// Check for --no-update flag
	if (process.argv.includes('--no-update')) {
		StartCompilerModule();
		return;
	}

	CheckModuleUpdates().then(StartCompilerModule);
};

Initialize();
