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

import chalk from 'chalk';
import { readFileSync } from 'fs';
import path, { dirname } from 'path';
import { fileURLToPath } from 'url';

declare const __GIT_COMMIT__: string;

const version: string = JSON.parse(readFileSync(path.resolve(dirname(fileURLToPath(import.meta.url)), '../package.json'), 'utf8')).version;

interface DoneOptions {
	elapsedMs: number;
	buildType: 'dev' | 'prod';
	sysfsCount?: number;
	envCount?: number;
}

const Logger = {
	warn(message: string, loc?: string) {
		if (loc) {
			console.warn(`${chalk.dim(loc + ':')} ${message}`);
		} else {
			console.warn(`${chalk.yellow('warning:')} ${message}`);
		}
	},

	error(message: string, loc?: string) {
		if (loc) {
			console.error(`${chalk.red(loc + ':')} ${message}`);
		} else {
			console.error(`${chalk.red('error:')} ${message}`);
		}
	},

	update(current: string, latest: string, installCmd: string) {
		console.log(`\n${chalk.yellow('update available')} ${chalk.dim(current)} → ${chalk.green(latest)}`);
		console.log(`${chalk.dim('run:')} ${installCmd}\n`);
	},

	done({ elapsedMs, buildType, sysfsCount, envCount }: DoneOptions) {
		const elapsed = `${(elapsedMs / 1000).toFixed(2)}s`;
		const meta: string[] = [`ttc v${version} (${__GIT_COMMIT__})`];
		if (buildType === 'dev') meta.push('no type checking');
		if (sysfsCount) meta.push(`${sysfsCount} bundled file${sysfsCount > 1 ? 's' : ''}`);
		if (envCount) meta.push(`${envCount} env var${envCount > 1 ? 's' : ''}`);
		console.log(`${chalk.green('Finished')} ${buildType} in ${elapsed} ` + chalk.dim(meta.join(', ')));
	},

	failed({ elapsedMs, buildType }: Pick<DoneOptions, 'elapsedMs' | 'buildType'>) {
		const elapsed = `${(elapsedMs / 1000).toFixed(2)}s`;
		console.error(`${chalk.red('Failed')} ${buildType} in ${elapsed} ` + chalk.dim(`(ttc v${version})`));
	},
};

export { Logger };
