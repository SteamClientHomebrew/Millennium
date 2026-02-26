import commonjs from '@rollup/plugin-commonjs';
import typescript from '@rollup/plugin-typescript';
import json from '@rollup/plugin-json';
import nodeResolve from '@rollup/plugin-node-resolve';
import replace from '@rollup/plugin-replace';
import { readFileSync } from 'fs';
import { execSync } from 'child_process';

const pkg = JSON.parse(readFileSync(new URL('./package.json', import.meta.url), 'utf8'));

let gitCommit = 'unknown';
try { gitCommit = execSync('git rev-parse --short HEAD', { encoding: 'utf8' }).trim(); } catch {}

export default {
	input: 'src/index.ts',
	context: 'window',
	output: {
		file: 'dist/index.js',
	},
	plugins: [replace({ __GIT_COMMIT__: JSON.stringify(gitCommit), preventAssignment: true }), commonjs(), typescript(), json(), nodeResolve()],
	external: [
		...Object.keys(pkg.dependencies || {}),
		...Object.keys(pkg.peerDependencies || {}),
		...Object.keys(pkg.optionalDependencies || {}),
		...Object.keys(pkg.devDependencies || {}),
	],
};
