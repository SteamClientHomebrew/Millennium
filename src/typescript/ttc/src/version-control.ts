import path from 'path';
import { fileURLToPath } from 'url';
import { readFile, access } from 'fs/promises';
import { dirname } from 'path';
import { Logger } from './logger';

async function fileExists(filePath: string): Promise<boolean> {
	return access(filePath).then(() => true).catch(() => false);
}

async function detectPackageManager(): Promise<string> {
	const userAgent = process.env.npm_config_user_agent ?? '';
	if (userAgent.startsWith('bun')) return 'bun';
	if (userAgent.startsWith('pnpm')) return 'pnpm';
	if (userAgent.startsWith('yarn')) return 'yarn';

	const cwd = process.cwd();
	if (await fileExists(path.join(cwd, 'bun.lock')) || await fileExists(path.join(cwd, 'bun.lockb'))) return 'bun';
	if (await fileExists(path.join(cwd, 'pnpm-lock.yaml'))) return 'pnpm';
	if (await fileExists(path.join(cwd, 'yarn.lock'))) return 'yarn';

	return 'npm';
}

function installCommand(pm: string, pkg: string): string {
	switch (pm) {
		case 'bun': return `bun add -d ${pkg}`;
		case 'pnpm': return `pnpm add -D ${pkg}`;
		case 'yarn': return `yarn add -D ${pkg}`;
		default: return `npm i -D ${pkg}`;
	}
}

export const CheckForUpdates = async (): Promise<boolean> => {
	try {
		const packageJsonPath = path.resolve(dirname(fileURLToPath(import.meta.url)), '../package.json');
		const packageJson = JSON.parse(await readFile(packageJsonPath, 'utf8'));

		const response = await fetch('https://registry.npmjs.org/@steambrew/ttc');
		const json = await response.json();
		const latest = json?.['dist-tags']?.latest;

		if (latest && latest !== packageJson.version) {
			const pm = await detectPackageManager();
			Logger.update(packageJson.version, latest, installCommand(pm, `@steambrew/ttc@${latest}`));
			return true;
		}
	} catch {
		// network or parse failure — silently skip
	}

	return false;
};
