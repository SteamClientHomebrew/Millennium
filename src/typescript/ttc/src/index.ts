#!/usr/bin/env node

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
