import path from 'path';
import del from 'rollup-plugin-del';
import terser from '@rollup/plugin-terser';
import typescript from '@rollup/plugin-typescript';

export default {
	input: 'src/index.ts',
	context: 'window',
	output: {
		file: 'build/cdp-isolated-ctx.js',
		format: 'iife',
		sourcemap: true,
	},
	onwarn(warning, warn) {
		throw new Error(warning.message);
	},
	plugins: [
		del({ targets: ['build/*', 'build/.*'], runOnce: true }),
		typescript(),
		terser(),
		{
			name: 'file-sourcemap-urls',
			generateBundle(options, bundle) {
				const outDir = path.resolve(options.dir || path.dirname(options.file || ''));
				for (const [fileName, chunk] of Object.entries(bundle)) {
					if (chunk.type === 'chunk' && chunk.code) {
						const absMapPath = path.resolve(outDir, fileName + '.map').replace(/\\/g, '/');
						chunk.code = chunk.code.replace(
							/\/\/# sourceMappingURL=\S+/,
							`//# sourceMappingURL=file:///${absMapPath}`,
						);
					}
				}
			},
		},
	],
};
