import replace from '@rollup/plugin-replace';
import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import terser from '@rollup/plugin-terser';
import typescript from '@rollup/plugin-typescript';
import externalGlobals from 'rollup-plugin-external-globals';
import del from 'rollup-plugin-del';

export default {
	input: 'src/index.ts',
	context: 'window',
	external: ['react', 'react-dom'],
	output: {
		dir: 'build',
		format: 'esm',
		entryFileNames: 'preload.js',
		chunkFileNames: (chunkInfo) => {
			return 'chunk-[hash].js';
		},
		sourcemap: true,
	},
	preserveEntrySignatures: true,
	treeshake: {
		pureExternalImports: false,
		preset: 'smallest',
	},
	plugins: [
		del({
			targets: ['build/**/*'],
		}),
		resolve(),
		commonjs(),
		externalGlobals({
			react: 'SP_REACT',
			'react-dom': 'SP_REACTDOM',
		}),
		typescript(),
		replace({
			preventAssignment: false,
			'process.env.NODE_ENV': JSON.stringify('production'),
		}),
		terser(),
	],
};
