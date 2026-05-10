import fs from 'fs';
import path from 'path';
import del from 'rollup-plugin-del';
import replace from '@rollup/plugin-replace';
import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import terser from '@rollup/plugin-terser';
import typescript from '@rollup/plugin-typescript';
import externalGlobals from 'rollup-plugin-external-globals';
import injectProcessEnv from 'rollup-plugin-inject-process-env';

const clientVersion = JSON.parse(fs.readFileSync(new URL('../client/package.json', import.meta.url))).version;
const browserVersion = JSON.parse(fs.readFileSync(new URL('../browser/package.json', import.meta.url))).version;

export default {
	input: 'src/index.ts',
	context: 'window',
	external: ['react', 'react-dom'],
	output: {
		dir: 'build',
		format: 'esm',
		entryFileNames: `millennium.js`,
		chunkFileNames: 'chunks/chunk-[name].js',
		sourcemap: true,
	},
	preserveEntrySignatures: true,
	treeshake: {
		pureExternalImports: false,
		preset: 'smallest',
	},
	onwarn(warning, warn) {
		throw new Error(warning.message);
	},
	plugins: [
		del({
			targets: ['build/*', 'build/.*'],
			runOnce: true,
		}),
		{
			name: 'load-sourcemaps',
			load(id) {
				if (!id.endsWith('.js')) return null;
				try {
					return {
						code: fs.readFileSync(id, 'utf8'),
						map: fs.readFileSync(id + '.map', 'utf8'),
					};
				} catch {
					return null;
				}
			},
		},
		injectProcessEnv({
			MILLENNIUM_FRONTEND_LIB_VERSION: clientVersion,
			MILLENNIUM_BROWSER_LIB_VERSION: browserVersion,
			MILLENNIUM_LOADER_BUILD_DATE: new Date().toISOString(),
		}),
		resolve(),
		commonjs(),
		externalGlobals({
			'react/jsx-runtime': 'SP_JSX_FACTORY',
			react: 'SP_REACT',
			'react-dom': 'SP_REACTDOM',
		}),
		typescript(),
		replace({
			preventAssignment: false,
			'process.env.NODE_ENV': JSON.stringify('production'),
		}),
		terser(),
		{
			name: 'file-sourcemap-urls',
			generateBundle(options, bundle) {
				const outDir = path.resolve(options.dir || path.dirname(options.file || ''));
				for (const [fileName, chunk] of Object.entries(bundle)) {
					if (chunk.type === 'chunk' && chunk.code) {
						const absMapPath = path.resolve(outDir, fileName + '.map').replace(/\\/g, '/');
						chunk.code = chunk.code.replace(
							/\/\/# sourceMappingURL=\S+\s*$/,
							`//# sourceMappingURL=file:///${absMapPath}`,
						);
					}
				}
			},
		},
	],
};
