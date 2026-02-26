import babel from '@rollup/plugin-babel';
import commonjs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import resolve, { nodeResolve } from '@rollup/plugin-node-resolve';
import replace from '@rollup/plugin-replace';
import terser from '@rollup/plugin-terser';
import typescript from '@rollup/plugin-typescript';
import esbuild from 'rollup-plugin-esbuild';
import url from '@rollup/plugin-url';
import nodePolyfills from 'rollup-plugin-polyfill-node';
import chalk from 'chalk';
import { InputPluginOption, OutputBundle, OutputOptions, Plugin, RollupOptions, rollup, watch as rollupWatch } from 'rollup';
import { minify_sync } from 'terser';
import scss from 'rollup-plugin-scss';
import * as sass from 'sass';
import fs from 'fs';
import path from 'path';
import { pathToFileURL } from 'url';
import dotenv from 'dotenv';
import injectProcessEnv from 'rollup-plugin-inject-process-env';
import { performance } from 'perf_hooks';
import { ExecutePluginModule, InitializePlugins } from './plugin-api';
import { Logger } from './logger';
import { PluginJson } from './plugin-json';
import constSysfsExpr from './static-embed';

const env = dotenv.config().parsed ?? {};

declare global {
	interface Window {
		PLUGIN_LIST: any;
		MILLENNIUM_PLUGIN_SETTINGS_STORE: any;
	}
}

declare const pluginName: string, millennium_main: any, MILLENNIUM_BACKEND_IPC: any, MILLENNIUM_IS_CLIENT_MODULE: boolean;

declare const MILLENNIUM_API: {
	callable: (fn: Function, route: string) => any;
	__INTERNAL_CALL_WEBKIT_METHOD__: (pluginName: string, methodName: string, kwargs: any) => any;
};

declare const __call_server_method__: (methodName: string, kwargs: any) => any;

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

const kCallServerMethod = 'const __call_server_method__ = (methodName, kwargs) => Millennium.callServerMethod(pluginName, methodName, kwargs)';

function __wrapped_callable__(route: string) {
	if (route.startsWith('webkit:')) {
		return MILLENNIUM_API.callable(
			(methodName: string, kwargs: any) => MILLENNIUM_API.__INTERNAL_CALL_WEBKIT_METHOD__(pluginName, methodName, kwargs),
			route.replace(/^webkit:/, ''),
		);
	}

	return MILLENNIUM_API.callable(__call_server_method__, route);
}

function wrapEntryPoint(code: string): string {
	return `let PluginEntryPointMain = function() { ${code} return millennium_main; };`;
}

function insertMillennium(target: BuildTarget, props: TranspilerProps): InputPluginOption {
	return {
		name: '',
		generateBundle(_: unknown, bundle: OutputBundle) {
			for (const fileName in bundle) {
				const chunk = bundle[fileName];
				if (chunk.type !== 'chunk') continue;

				let code = `\
const MILLENNIUM_IS_CLIENT_MODULE = ${target === BuildTarget.Plugin};
const pluginName = "${props.pluginName}";
${InitializePlugins.toString()}
${InitializePlugins.name}()
${kCallServerMethod}
${__wrapped_callable__.toString()}
${wrapEntryPoint(chunk.code)}
${ExecutePluginModule.toString()}
${ExecutePluginModule.name}()`;

				if (props.minify) {
					code = minify_sync(code).code ?? code;
				}

				chunk.code = code;
			}
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

interface PathEntry {
	pattern: string;
	targets: string[];
	configDir: string;
}

/**
 * tsconfig.json files use JSONC, not regular JSON.
 * JSONC supports comments and trailing commas, while JSON does not.
 * This function sanitizes JSONC into JSON.parse()-able content.
 *
 * @param text input text
 * @returns object
 */
function parseJsonc(text: string): any {
	let out = '';
	let i = 0;
	const n = text.length;
	while (i < n) {
		const ch = text[i];
		if (ch === '"') {
			out += ch;
			i++;
			while (i < n) {
				const c = text[i];
				out += c;
				if (c === '\\') {
					i++;
					if (i < n) {
						out += text[i];
						i++;
					}
				} else if (c === '"') {
					i++;
					break;
				} else i++;
			}
		} else if (ch === '/' && i + 1 < n && text[i + 1] === '/') {
			i += 2;
			while (i < n && text[i] !== '\n') i++;
		} else if (ch === '/' && i + 1 < n && text[i + 1] === '*') {
			i += 2;
			while (i < n - 1 && !(text[i] === '*' && text[i + 1] === '/')) i++;
			i += 2;
		} else if (ch === ',') {
			let j = i + 1;
			while (j < n && (text[j] === ' ' || text[j] === '\t' || text[j] === '\n' || text[j] === '\r')) j++;
			if (j < n && (text[j] === '}' || text[j] === ']')) {
				i++;
			} else {
				out += ch;
				i++;
			}
		} else {
			out += ch;
			i++;
		}
	}
	return JSON.parse(out);
}

function tsconfigPathsPlugin(tsconfigPath: string): InputPluginOption {
	function readConfig(cfgPath: string): { baseUrl: string | null; entries: PathEntry[] } {
		const dir = path.dirname(path.resolve(cfgPath));
		let raw: any;
		try {
			raw = parseJsonc(fs.readFileSync(cfgPath, 'utf8'));
		} catch {
			return { baseUrl: null, entries: [] };
		}

		let parentResult: { baseUrl: string | null; entries: PathEntry[] } = { baseUrl: null, entries: [] };
		if (raw.extends) {
			const ext = raw.extends as string;
			const parentPath = path.resolve(dir, ext.endsWith('.json') ? ext : `${ext}.json`);
			parentResult = readConfig(parentPath);
		}

		const opts = raw.compilerOptions ?? {};
		const baseUrl = opts.baseUrl ? path.resolve(dir, opts.baseUrl as string) : parentResult.baseUrl;

		const ownEntries: PathEntry[] = Object.entries(opts.paths ?? {}).map(([pattern, targets]) => ({
			pattern,
			targets: targets as string[],
			configDir: dir,
		}));

		const ownPatterns = new Set(ownEntries.map((e) => e.pattern));
		const parentEntries = parentResult.entries.filter((e) => !ownPatterns.has(e.pattern));

		return { baseUrl, entries: [...ownEntries, ...parentEntries] };
	}

	const { baseUrl, entries } = readConfig(tsconfigPath);

	function resolveWithExtensions(base: string): string | null {
		for (const ext of ['', '.ts', '.tsx', '.js', '.jsx', '/index.ts', '/index.tsx']) {
			if (fs.existsSync(base + ext)) return base + ext;
		}
		return null;
	}

	return {
		name: 'tsconfig-paths',
		async resolveId(source: string, importer: string | undefined) {
			for (const { pattern, targets, configDir } of entries) {
				if (!targets.length) continue;
				const isWild = pattern.endsWith('/*');
				if (isWild) {
					const prefix = pattern.slice(0, -2);
					if (source === prefix || source.startsWith(prefix + '/')) {
						const rest = source.startsWith(prefix + '/') ? source.slice(prefix.length + 1) : '';
						const targetBase = path.resolve(configDir, targets[0].replace('*', rest));
						const resolved = resolveWithExtensions(targetBase);
						if (resolved) {
							const result = await this.resolve(resolved, importer, { skipSelf: true });
							if (result) return result;
						}
					}
				} else if (source === pattern) {
					const targetBase = path.resolve(configDir, targets[0]);
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
			insertMillennium(BuildTarget.Plugin, this.props),
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
				'Millennium.callServerMethod': '__call_server_method__',
				'client.callable': '__wrapped_callable__',
				'client.pluginSelf': 'window.PLUGIN_LIST[pluginName]',
				'client.Millennium.exposeObj(': 'client.Millennium.exposeObj(exports, ',
				'client.BindPluginSettings()': 'client.BindPluginSettings(pluginName)',
			}),
			...(Object.keys(env).length > 0 ? [injectProcessEnv(env)] : []),
			...(this.props.minify ? [terser()] : []),
		];
	}

	protected output(isMillennium: boolean): OutputOptions {
		return {
			name: 'millennium_main',
			file: isMillennium ? '../../build/frontend.bin' : '.millennium/Dist/index.js',
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
			insertMillennium(BuildTarget.Webkit, this.props),
			tsPlugin,
			url({ include: ['**/*.mp4', '**/*.webm', '**/*.ogg'], limit: 0, fileName: '[name][extname]', destDir: 'dist/assets' }),
			resolve(),
			commonjs(),
			json(),
			sysfsPlugin,
			replace({
				delimiters: ['', ''],
				preventAssignment: true,
				'Millennium.callServerMethod': '__call_server_method__',
				'webkit.callable': '__wrapped_callable__',
				'webkit.Millennium.exposeObj(': 'webkit.Millennium.exposeObj(exports, ',
				'client.BindPluginSettings()': 'client.BindPluginSettings(pluginName)',
			}),
			...(this.props.minify ? [babel({ presets: ['@babel/preset-env', '@babel/preset-react'], babelHelpers: 'bundled' })] : []),
			...(Object.keys(env).length > 0 ? [injectProcessEnv(env)] : []),
		];

		const merged = await withUserPlugins(base);
		return this.props.minify ? [...merged, terser()] : merged;
	}

	protected output(_isMillennium: boolean): OutputOptions {
		return {
			name: 'millennium_main',
			file: '.millennium/Dist/webkit.js',
			exports: 'named',
			format: 'iife',
			globals: { '@steambrew/webkit': 'window.MILLENNIUM_API' },
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
