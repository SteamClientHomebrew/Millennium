import chalk from 'chalk';
import { readFileSync } from 'fs';
import path, { dirname } from 'path';
import { fileURLToPath } from 'url';

const version: string = JSON.parse(
	readFileSync(path.resolve(dirname(fileURLToPath(import.meta.url)), '../package.json'), 'utf8'),
).version;

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
		const meta: string[] = [`ttc v${version}`];
		if (buildType === 'dev') meta.push('no type checking');
		if (sysfsCount) meta.push(`${sysfsCount} constSysfsExpr`);
		if (envCount) meta.push(`${envCount} env var${envCount > 1 ? 's' : ''}`);
		console.log(`${chalk.green('Finished')} ${buildType} in ${elapsed} ` + chalk.dim('(' + meta.join(', ') + ')'));
	},
};

export { Logger };
