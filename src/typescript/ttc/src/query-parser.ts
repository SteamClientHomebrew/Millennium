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

import { Logger } from './logger';

export const PrintParamHelp = () => {
	console.log(
		[
			'',
			'usage: millennium-ttc --build <dev|prod> [options]',
			'',
			'options:',
			'  --build <dev|prod>   build type (prod enables minification)',
			'  --target <path>      plugin directory (default: current directory)',
			'  --watch              enable watch mode for continuous rebuilding',
			'  --no-update          skip update check',
			'  --help               show this message',
			'',
		].join('\n'),
	);
};

export enum BuildType {
	DevBuild,
	ProdBuild,
}

export interface ParameterProps {
	type: BuildType;
	targetPlugin: string; // path
	isMillennium?: boolean;
	watch?: boolean;
}

export const ValidateParameters = (args: Array<string>): ParameterProps => {
	let typeProp: BuildType = BuildType.DevBuild,
		targetProp: string = process.cwd(),
		isMillennium: boolean = false,
		watch: boolean = false;

	if (args.includes('--help')) {
		PrintParamHelp();
		process.exit();
	}

	if (!args.includes('--build')) {
		Logger.error('missing required argument: --build');
		PrintParamHelp();
		process.exit();
	}

	for (let i = 0; i < args.length; i++) {
		if (args[i] === '--build') {
			const BuildMode: string = args[i + 1];

			switch (BuildMode) {
				case 'dev':
					typeProp = BuildType.DevBuild;
					break;
				case 'prod':
					typeProp = BuildType.ProdBuild;
					break;
				default: {
					Logger.error('--build must be one of: dev, prod');
					process.exit();
				}
			}
		}

		if (args[i] == '--target') {
			if (args[i + 1] === undefined) {
				Logger.error('--target requires a path argument');
				process.exit();
			}

			targetProp = args[i + 1];
		}

		if (args[i] == '--millennium-internal') {
			isMillennium = true;
		}

		if (args[i] === '--watch') {
			watch = true;
		}
	}

	return {
		type: typeProp,
		targetPlugin: targetProp,
		isMillennium,
		watch,
	};
};
