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

import babel from '@rollup/plugin-babel';
import commonjs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import resolve, { nodeResolve } from '@rollup/plugin-node-resolve';
import replace from '@rollup/plugin-replace';
import terser from '@rollup/plugin-terser';
import typescript from '@rollup/plugin-typescript';
import esbuild from 'rollup-plugin-esbuild';
import ts from 'typescript';
import url from '@rollup/plugin-url';
import nodePolyfills from 'rollup-plugin-polyfill-node';
import chalk from 'chalk';
import { InputPluginOption, OutputOptions, Plugin, RollupOptions, rollup, watch as rollupWatch } from 'rollup';
import scss from 'rollup-plugin-scss';
import * as sass from 'sass';
import fs from 'fs';
import path from 'path';
import { pathToFileURL } from 'url';
import dotenv from 'dotenv';
import injectProcessEnv from 'rollup-plugin-inject-process-env';
import { performance } from 'perf_hooks';
import { parse } from '@babel/parser';
import _generate from '@babel/generator';
import _template from '@babel/template';
import * as t from '@babel/types';
import { Logger } from './logger';
import { PluginJson } from './plugin-json';
import constSysfsExpr from './static-embed';
import astTransforms, { type Transform } from './ast-transforms';

const generate = (typeof _generate === 'function' ? _generate : (_generate as any).default) as typeof _generate;
const template = (typeof _template === 'function' ? _template : (_template as any).default) as typeof _template;

const env = dotenv.config().parsed ?? {};

export interface TranspilerProps {
	minify: boolean;
	pluginName: string;
	watch?: boolean;
	isMillennium?: boolean;
}

enum BuildTarget {
	Plugin,
	Webkit,
}

const buildPluginWrapper = template.statements({
	syntacticPlaceholders: true,
})(`
	const MILLENNIUM_IS_CLIENT_MODULE = %%isClient%%;
	const pluginName = %%pluginName%%;
	(window.PLUGIN_LIST ||= {})[pluginName] ||= {};
	window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS ||= {};

	let PluginEntryPointMain = function () {
		%%chunkBody%%
		return millennium_main;
	};

	(async () => {
		const PluginModule = PluginEntryPointMain();
		Object.assign(window.PLUGIN_LIST[pluginName], {
			...PluginModule,
			__millennium_internal_plugin_name_do_not_use_or_change__: pluginName,
		});
		const pluginProps = await PluginModule.default();
		if (
			pluginProps &&
			pluginProps.title !== undefined &&
			pluginProps.icon !== undefined &&
			pluginProps.content !== undefined
		) {
			window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS[pluginName] = pluginProps;
		}
		if (MILLENNIUM_IS_CLIENT_MODULE) {
			MILLENNIUM_BACKEND_IPC.postMessage(1, { pluginName: pluginName });
		}
	})();
`);

function insertMillennium(target: BuildTarget, props: TranspilerProps): InputPluginOption {
	return {
		name: 'insert-millennium',
		renderChunk(code, chunk) {
			const chunkAst = parse(code, { sourceType: 'script', plugins: ['jsx'] });
			const wrapped = buildPluginWrapper({
				isClient: t.booleanLiteral(target === BuildTarget.Plugin),
				pluginName: t.stringLiteral(props.pluginName),
				chunkBody: chunkAst.program.body,
			});

			const program = t.program(Array.isArray(wrapped) ? wrapped : [wrapped]);
			const result = generate(program, { sourceMaps: true, sourceFileName: chunk.fileName });
			return { code: result.code, map: result.map };
		},
	};
}

function getFrontendDir(pluginJson: any): string {
	return pluginJson?.frontend ?? 'frontend';
}

function resolveEntryFile(frontendDir: string): string {
	return frontendDir === '.' || frontendDir === './' || frontendDir === '' ? './index.tsx' : `./${frontendDir}/index.tsx`;
}

function resolveTsConfig(frontendDir: string): string {
	if (frontendDir === '.' || frontendDir === './') return './tsconfig.json';
	const candidate = `./${frontendDir}/tsconfig.json`;
	return fs.existsSync(candidate) ? candidate : './tsconfig.json';
}

async function withUserPlugins(plugins: InputPluginOption[]): Promise<InputPluginOption[]> {
	if (!fs.existsSync('./ttc.config.mjs')) return plugins;
	const configUrl = pathToFileURL(path.resolve('./ttc.config.mjs')).href;
	const { MillenniumCompilerPlugins } = await import(configUrl);
	const deduped = (MillenniumCompilerPlugins as Plugin<any>[]).filter((custom) => !plugins.some((p) => (p as Plugin<any>)?.name === custom?.name));
	return [...plugins, ...deduped];
}

function logLocation(log: { loc?: { file?: string; line: number; column: number }; id?: string }): string | undefined {
	const file = log.loc?.file ?? log.id;
	if (!file || !log.loc) return undefined;
	return `${path.relative(process.cwd(), file)}:${log.loc.line}:${log.loc.column}`;
}

function stripPluginPrefix(message: string): string {
	message = message.replace(/^\[plugin [^\]]+\]\s*/, '');
	message = message.replace(/^\S[^(]*\(\d+:\d+\):\s*/, '');
	message = message.replace(/^@?[\w-]+\/[\w-]+\s+/, '');
	return message;
}

class BuildFailedError extends Error {}

function tsconfigPathsPlugin(tsconfigPath: string): InputPluginOption {
	const absPath = path.resolve(tsconfigPath);
	const configDir = path.dirname(absPath);

	const { config, error } = ts.readConfigFile(absPath, ts.sys.readFile);
	if (error) return { name: 'tsconfig-paths' };

	const { options } = ts.parseJsonConfigFileContent(config, ts.sys, configDir);
	const baseUrl = options.baseUrl ?? null;
	const pathsBase = (options.pathsBasePath as string | undefined) ?? configDir;
	const paths = options.paths ?? {};

	function resolveWithExtensions(base: string): string | null {
		for (const ext of ['', '.ts', '.tsx', '.js', '.jsx', '/index.ts', '/index.tsx']) {
			if (fs.existsSync(base + ext) && fs.statSync(base + ext).isFile()) return base + ext;
		}
		return null;
	}

	return {
		name: 'tsconfig-paths',
		async resolveId(source: string, importer: string | undefined) {
			for (const [pattern, targets] of Object.entries(paths)) {
				if (!targets.length) continue;
				const isWild = pattern.endsWith('/*');
				if (isWild) {
					const prefix = pattern.slice(0, -2);
					if (source === prefix || source.startsWith(prefix + '/')) {
						const rest = source.startsWith(prefix + '/') ? source.slice(prefix.length + 1) : '';
						const targetBase = path.resolve(pathsBase, targets[0].replace('*', rest));
						const resolved = resolveWithExtensions(targetBase);
						if (resolved) {
							const result = await this.resolve(resolved, importer, { skipSelf: true });
							if (result) return result;
						}
					}
				} else if (source === pattern) {
					const targetBase = path.resolve(pathsBase, targets[0]);
					const resolved = resolveWithExtensions(targetBase);
					if (resolved) {
						const result = await this.resolve(resolved, importer, { skipSelf: true });
						if (result) return result;
					}
				}
			}

			if (baseUrl && !source.startsWith('.') && !source.startsWith('/') && !source.startsWith('\0') && !source.startsWith('@')) {
				const resolved = resolveWithExtensions(path.resolve(baseUrl, source));
				if (resolved) {
					const result = await this.resolve(resolved, importer, { skipSelf: true });
					if (result) return result;
				}
			}

			return null;
		},
	};
}

const PLUGIN_NAME_INJECTIONS: Transform[] = [
	{ type: 'inject_arg', match: ['client', 'BindPluginSettings'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'pluginConfig', 'get'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'pluginConfig', 'set'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'pluginConfig', 'delete'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'pluginConfig', 'getAll'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'usePluginConfig'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'subscribePluginConfig'], arg: 'pluginName' },
];

const FRONTEND_TRANSFORMS: Transform[] = [
	/* override pluginSelf */
	{ type: 'rename', match: ['client', 'pluginSelf'], replacement: 'window.PLUGIN_LIST[pluginName]' },
	/* inject plugin name */
	{ type: 'inject_arg', match: ['client', 'callable'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'ffi'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'Millennium', 'callServerMethod'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['client', 'Millennium', 'exposeObj'], arg: 'exports' },
	...PLUGIN_NAME_INJECTIONS,
	/* hoist ChromeDevToolsProtocol to a per-plugin instance; falls back to the shared singleton on old SDKs */
	{
		type: 'inject_const',
		match: ['client', 'ChromeDevToolsProtocol'],
		localName: 'ChromeDevToolsProtocol',
		init: 'client.MillenniumChromeDevToolsProtocol ? new client.MillenniumChromeDevToolsProtocol(pluginName) : client.ChromeDevToolsProtocol',
	},
];

const WEBKIT_TRANSFORMS: Transform[] = [
	/* inject plugin name */
	{ type: 'inject_arg', match: ['webkit', 'callable'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['webkit', 'Millennium', 'callServerMethod'], arg: 'pluginName' },
	{ type: 'inject_arg', match: ['webkit', 'Millennium', 'exposeObj'], arg: 'exports' },
	...PLUGIN_NAME_INJECTIONS,
];

abstract class MillenniumBuild {
	protected abstract readonly externals: ReadonlySet<string>;
	protected abstract readonly forbidden: ReadonlyMap<string, string>;
	protected abstract plugins(sysfsPlugin: InputPluginOption): InputPluginOption[] | Promise<InputPluginOption[]>;
	protected abstract output(isMillennium: boolean): OutputOptions;

	protected isExternal(id: string): boolean {
		const hint = this.forbidden.get(id);
		if (hint) {
			Logger.error(`${id} cannot be used here; ${hint}`);
			process.exit(1);
		}
		return this.externals.has(id);
	}

	async watchConfig(input: string, sysfsPlugin: InputPluginOption, isMillennium: boolean): Promise<RollupOptions> {
		return {
			input,
			plugins: await this.plugins(sysfsPlugin),
			onwarn: (warning) => {
				const msg = stripPluginPrefix(warning.message);
				const loc = logLocation(warning);
				if (warning.plugin === 'typescript') {
					Logger.error(msg, loc);
				} else {
					Logger.warn(msg, loc);
				}
			},
			context: 'window',
			external: (id) => this.isExternal(id),
			output: this.output(isMillennium),
		};
	}

	async build(input: string, sysfsPlugin: InputPluginOption, isMillennium: boolean): Promise<void> {
		let hasErrors = false;

		const config: RollupOptions = {
			input,
			plugins: await this.plugins(sysfsPlugin),
			onwarn: (warning) => {
				const msg = stripPluginPrefix(warning.message);
				const loc = logLocation(warning);
				if (warning.plugin === 'typescript') {
					Logger.error(msg, loc);
					hasErrors = true;
				} else {
					Logger.warn(msg, loc);
				}
			},
			context: 'window',
			external: (id) => this.isExternal(id),
			output: this.output(isMillennium),
		};

		await (await rollup(config)).write(config.output as OutputOptions);

		if (hasErrors) throw new BuildFailedError();
	}
}

class FrontendBuild extends MillenniumBuild {
	protected readonly externals = new Set(['@steambrew/client', 'react', 'react-dom', 'react-dom/client', 'react/jsx-runtime']);
	protected readonly forbidden = new Map([['@steambrew/webkit', 'use @steambrew/client in the frontend module']]);

	constructor(
		private readonly frontendDir: string,
		private readonly props: TranspilerProps,
	) {
		super();
	}

	protected plugins(sysfsPlugin: InputPluginOption): InputPluginOption[] {
		const tsconfigPath = resolveTsConfig(this.frontendDir);
		const tsPlugin = this.props.minify
			? typescript({ tsconfig: tsconfigPath, compilerOptions: { outDir: undefined } })
			: esbuild({ tsconfig: tsconfigPath, target: 'esnext', jsx: 'automatic' });

		return [
			...(this.props.minify ? [] : [tsconfigPathsPlugin(tsconfigPath)]),
			tsPlugin,
			url({ include: ['**/*.gif', '**/*.webm', '**/*.svg'], limit: 0, fileName: '[hash][extname]' }),
			nodeResolve({ browser: true }),
			commonjs(),
			nodePolyfills(),
			scss({ output: false, outputStyle: 'compressed', sourceMap: false, watch: 'src/styles', sass }),
			json(),
			sysfsPlugin,
			replace({
				delimiters: ['', ''],
				preventAssignment: true,
				'process.env.NODE_ENV': JSON.stringify('production'),
			}),
			astTransforms(FRONTEND_TRANSFORMS),
			insertMillennium(BuildTarget.Plugin, this.props),
			...(Object.keys(env).length > 0 ? [injectProcessEnv(env)] : []),
			terser(),
		];
	}

	protected output(isMillennium: boolean): OutputOptions {
		const outFile = isMillennium ? '../.frontend.bin' : '.millennium/Dist/index.js';
		const opts: OutputOptions = {
			name: 'millennium_main',
			file: outFile,
			globals: {
				react: 'window.SP_REACT',
				'react-dom': 'window.SP_REACTDOM',
				'react-dom/client': 'window.SP_REACTDOM',
				'react/jsx-runtime': 'SP_JSX_FACTORY',
				'@steambrew/client': 'window.MILLENNIUM_API',
			},
			exports: 'named',
			format: 'iife',
		};

		if (!this.props.minify) {
			opts.sourcemap = true;
			if (isMillennium) {
				const absDir = path.resolve(path.dirname(outFile)).replace(/\\/g, '/');
				opts.sourcemapBaseUrl = `file:///${absDir}`;
			}
		}

		return opts;
	}
}

class WebkitBuild extends MillenniumBuild {
	protected readonly externals = new Set(['@steambrew/webkit']);
	protected readonly forbidden = new Map([['@steambrew/client', 'use @steambrew/webkit in the webkit module']]);

	constructor(private readonly props: TranspilerProps) {
		super();
	}

	protected async plugins(sysfsPlugin: InputPluginOption): Promise<InputPluginOption[]> {
		const webkitTsconfig = './webkit/tsconfig.json';
		const tsPlugin = this.props.minify ? typescript({ tsconfig: webkitTsconfig }) : esbuild({ tsconfig: webkitTsconfig, target: 'esnext', jsx: 'automatic' });

		const base: InputPluginOption[] = [
			...(this.props.minify ? [] : [tsconfigPathsPlugin(webkitTsconfig)]),
			tsPlugin,
			url({ include: ['**/*.mp4', '**/*.webm', '**/*.ogg'], limit: 0, fileName: '[name][extname]', destDir: 'dist/assets' }),
			resolve(),
			commonjs(),
			json(),
			sysfsPlugin,
			astTransforms(WEBKIT_TRANSFORMS),
			insertMillennium(BuildTarget.Webkit, this.props),
			...(this.props.minify ? [babel({ presets: ['@babel/preset-env', '@babel/preset-react'], babelHelpers: 'bundled' })] : []),
			...(Object.keys(env).length > 0 ? [injectProcessEnv(env)] : []),
		];

		const merged = await withUserPlugins(base);
		return [...merged, terser()];
	}

	protected output(_isMillennium: boolean): OutputOptions {
		return {
			name: 'millennium_main',
			file: '.millennium/Dist/webkit.js',
			exports: 'named',
			format: 'iife',
			globals: { '@steambrew/webkit': 'window.MILLENNIUM_API' },
			...(!this.props.minify && { sourcemap: true as const }),
		};
	}
}

function RunWatchMode(frontendConfig: RollupOptions, webkitConfig: RollupOptions | null): void {
	const configs = webkitConfig ? [frontendConfig, webkitConfig] : [frontendConfig];
	const watcher = rollupWatch(configs);

	console.log(chalk.blueBright.bold('watch'), 'watching for file changes...');

	watcher.on('event', async (event) => {
		if (event.code === 'BUNDLE_START') {
			const label = (event.output as readonly string[]).some((f) => f.includes('index.js')) ? 'frontend' : 'webkit';
			console.log(chalk.yellowBright.bold('watch'), `rebuilding ${label}...`);
		} else if (event.code === 'BUNDLE_END') {
			const label = (event.output as readonly string[]).some((f) => f.includes('index.js')) ? 'frontend' : 'webkit';
			console.log(chalk.greenBright.bold('watch'), `${label} built in ${chalk.green(`${event.duration}ms`)}`);
			await event.result.close();
		} else if (event.code === 'ERROR') {
			const err = event.error;
			const msg = stripPluginPrefix(err?.message ?? String(err));
			Logger.error(msg, logLocation(err as any));
			if (event.result) await event.result.close();
		}
	});

	const shutdown = () => {
		console.log(chalk.yellowBright.bold('watch'), 'stopping...');
		watcher.close();
		process.exit(0);
	};

	process.on('SIGINT', shutdown);
	process.on('SIGUSR2', shutdown);
}

export const TranspilerPluginComponent = async (pluginJson: PluginJson, props: TranspilerProps) => {
	const webkitDir = './webkit/index.tsx';
	const frontendDir = getFrontendDir(pluginJson);
	const sysfs = constSysfsExpr();
	const isMillennium = props.isMillennium ?? false;

	if (props.watch) {
		const frontendConfig = await new FrontendBuild(frontendDir, props).watchConfig(resolveEntryFile(frontendDir), sysfs.plugin, isMillennium);
		const webkitConfig = fs.existsSync(webkitDir) ? await new WebkitBuild(props).watchConfig(webkitDir, sysfs.plugin, isMillennium) : null;
		RunWatchMode(frontendConfig, webkitConfig);
		return;
	}

	try {
		await new FrontendBuild(frontendDir, props).build(resolveEntryFile(frontendDir), sysfs.plugin, isMillennium);

		if (fs.existsSync(webkitDir)) {
			await new WebkitBuild(props).build(webkitDir, sysfs.plugin, isMillennium);
		}

		Logger.done({
			elapsedMs: performance.now() - global.PerfStartTime,
			buildType: props.minify ? 'prod' : 'dev',
			sysfsCount: sysfs.getCount() || undefined,
			envCount: Object.keys(env).length || undefined,
		});
	} catch (exception: any) {
		if (!(exception instanceof BuildFailedError)) {
			Logger.error(stripPluginPrefix(exception?.message ?? String(exception)), logLocation(exception));
		}
		Logger.failed({ elapsedMs: performance.now() - global.PerfStartTime, buildType: props.minify ? 'prod' : 'dev' });
		process.exit(1);
	}
};
