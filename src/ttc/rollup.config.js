import commonjs from '@rollup/plugin-commonjs';
import typescript from '@rollup/plugin-typescript';
import json from '@rollup/plugin-json';
import nodeResolve from '@rollup/plugin-node-resolve';
import { readFileSync } from 'fs';

const pkg = JSON.parse(readFileSync(new URL('./package.json', import.meta.url), 'utf8'));

export default {
	input: 'src/index.ts',
	context: 'window',
	output: {
		file: 'dist/index.js',
	},
	plugins: [commonjs(), typescript(), json(), nodeResolve()],
	external: [
		...Object.keys(pkg.dependencies || {}),
		...Object.keys(pkg.peerDependencies || {}),
		...Object.keys(pkg.optionalDependencies || {}),
		...Object.keys(pkg.devDependencies || {}),
	],
};
