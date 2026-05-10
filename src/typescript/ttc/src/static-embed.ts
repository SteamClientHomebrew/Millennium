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

import * as parser from '@babel/parser';
import { createFilter } from '@rollup/pluginutils';
import fs from 'fs';
import * as glob from 'glob';
import MagicString from 'magic-string';
import path from 'path';
import _traverse from '@babel/traverse';
import { Plugin, SourceDescription, TransformPluginContext } from 'rollup';

export interface SysfsPlugin {
	plugin: Plugin;
	getCount: () => number;
}

const traverse = (typeof _traverse === 'function' ? _traverse : (_traverse as any).default) as typeof _traverse;

interface EmbedPluginOptions {
	include?: string | RegExp | (string | RegExp)[];
	exclude?: string | RegExp | (string | RegExp)[];
	encoding?: BufferEncoding;
}

interface CallOptions {
	basePath?: string;
	include?: string;
	encoding?: BufferEncoding;
}

interface FileInfo {
	content: string;
	filePath: string;
	fileName: string;
}

interface ImportSpecifierInfo {
	specifierStart: number;
	specifierEnd: number;
	declStart: number;
	declEnd: number;
	isOnlySpecifier: boolean;
	prevSpecifierEnd: number | null;
	nextSpecifierStart: number | null;
}

export default function constSysfsExpr(options: EmbedPluginOptions = {}): SysfsPlugin {
	const filter = createFilter(options.include, options.exclude);
	const pluginName = 'millennium-const-sysfs-expr';
	let count = 0;
	const globCache = new Map<string, string>();

	const plugin: Plugin = {
		name: pluginName,

		transform(this: TransformPluginContext, code: string, id: string): SourceDescription | null {
			if (!filter(id)) return null;
			if (!code.includes('constSysfsExpr')) return null;

			const magicString = new MagicString(code);
			let hasReplaced = false;
			let constSysfsImport: ImportSpecifierInfo | null = null;

			try {
				const stringVariables = new Map<string, string>();

				const ast = parser.parse(code, {
					sourceType: 'module',
					plugins: ['typescript', 'jsx', 'objectRestSpread', 'classProperties', 'optionalChaining', 'nullishCoalescingOperator'],
				});

				traverse(ast, {
					VariableDeclarator(path) {
						const init = path.node.init;
						const id = path.node.id;
						if (id.type === 'Identifier' && init && init.type === 'StringLiteral') {
							stringVariables.set(id.name, init.value);
						}
					},
					ImportDeclaration(nodePath) {
						const decl = nodePath.node;
						const specifiers = decl.specifiers;
						const idx = specifiers.findIndex(
							(s) => s.type === 'ImportSpecifier' && ((s as any).imported?.name === 'constSysfsExpr' || (s as any).local?.name === 'constSysfsExpr'),
						);
						if (idx !== -1 && typeof decl.start === 'number' && typeof decl.end === 'number') {
							const spec = specifiers[idx];
							if (typeof spec.start === 'number' && typeof spec.end === 'number') {
								constSysfsImport = {
									specifierStart: spec.start,
									specifierEnd: spec.end,
									declStart: decl.start,
									declEnd: decl.end,
									isOnlySpecifier: specifiers.length === 1,
									prevSpecifierEnd: idx > 0 && typeof specifiers[idx - 1].end === 'number' ? (specifiers[idx - 1].end as number) : null,
									nextSpecifierStart:
										idx < specifiers.length - 1 && typeof specifiers[idx + 1].start === 'number' ? (specifiers[idx + 1].start as number) : null,
								};
							}
						}
					},
					CallExpression: (nodePath) => {
						const node = nodePath.node;
						if (node.callee.type === 'Identifier' && node.callee.name === 'constSysfsExpr') {
							if (typeof node.start !== 'number' || typeof node.end !== 'number') {
								if (node.loc) {
									this.warn(`Missing start/end offset info for constSysfsExpr call.`, node.loc.start.index);
								}
								return;
							}

							const args = node.arguments;
							let pathOrPattern: string | null = null;
							const callOptions: Required<Omit<CallOptions, 'fileName'>> = {
								basePath: '',
								include: '**/*',
								encoding: options.encoding || 'utf8',
							};

							if (args.length >= 1 && (args[0].type === 'StringLiteral' || args[0].type === 'Identifier')) {
								const firstArg = args[0];
								if (firstArg.type === 'StringLiteral') {
									pathOrPattern = firstArg.value;
								} else if (firstArg.type === 'Identifier') {
									const varName = firstArg.name;
									if (stringVariables.has(varName)) {
										pathOrPattern = stringVariables.get(varName) || null;
									} else {
										this.warn(
											`Unable to resolve variable "${varName}" for constSysfsExpr path/pattern. Only simple string literal assignments are supported.`,
											firstArg.loc?.start.index,
										);
										return;
									}
								}

								if (args.length > 1 && args[1].type === 'ObjectExpression') {
									const optionsObj = args[1];
									for (const prop of optionsObj.properties) {
										if (prop.type !== 'ObjectProperty') continue;
										let keyName: string | undefined;
										if (prop.key.type === 'Identifier') keyName = prop.key.name;
										else if (prop.key.type === 'StringLiteral') keyName = prop.key.value;
										else continue;

										if (!['basePath', 'include', 'encoding'].includes(keyName)) continue;

										const valueNode = prop.value;
										if (valueNode.type === 'StringLiteral') {
											const value = (valueNode as any).extra?.rawValue !== undefined ? (valueNode as any).extra.rawValue : valueNode.value;
											if (keyName === 'basePath') callOptions.basePath = value;
											else if (keyName === 'include') callOptions.include = value;
											else if (keyName === 'encoding') callOptions.encoding = value as BufferEncoding;
										} else {
											this.warn(
												`Option "${keyName}" for constSysfsExpr must be a string literal. Found type: ${valueNode.type}`,
												valueNode.loc?.start.index,
											);
										}
									}
								}
							} else if (args.length >= 1 && args[0].type === 'ObjectExpression') {
								const optionsObj = args[0];
								for (const prop of optionsObj.properties) {
									if (prop.type !== 'ObjectProperty') continue;
									let keyName: string | undefined;
									if (prop.key.type === 'Identifier') keyName = prop.key.name;
									else if (prop.key.type === 'StringLiteral') keyName = prop.key.value;
									else continue;

									// In this case, we need to look for 'basePath' and 'include' within the options object itself
									if (!['basePath', 'include', 'encoding'].includes(keyName)) continue;

									const valueNode = prop.value;
									if (valueNode.type === 'StringLiteral') {
										const value = (valueNode as any).extra?.rawValue !== undefined ? (valueNode as any).extra.rawValue : valueNode.value;
										if (keyName === 'basePath') callOptions.basePath = value;
										else if (keyName === 'include') callOptions.include = value;
										else if (keyName === 'encoding') callOptions.encoding = value as BufferEncoding;
									} else {
										this.warn(
											`Option "${keyName}" for constSysfsExpr must be a string literal. Found type: ${valueNode.type}`,
											valueNode.loc?.start.index,
										);
									}
								}

								if (callOptions.include !== '**/*') {
									pathOrPattern = callOptions.include;
								} else {
									if (!callOptions.basePath) {
										this.warn(
											`constSysfsExpr called with only an options object requires at least 'include' or 'basePath' for a pattern.`,
											node.loc?.start.index,
										);
										return;
									}
									this.warn(`constSysfsExpr called with only an options object requires an explicit 'include' pattern.`, node.loc?.start.index);
									return;
								}
							} else {
								this.warn(`constSysfsExpr requires a path/pattern string/variable or an options object as the first argument.`, node.loc?.start.index);
								return;
							}

							if (!pathOrPattern) {
								this.warn(`Invalid or unresolved path/pattern argument for constSysfsExpr.`, args[0]?.loc?.start.index);
								return;
							}

							try {
								const searchBasePath = callOptions.basePath
									? path.isAbsolute(callOptions.basePath)
										? callOptions.basePath
										: path.resolve(path.dirname(id), callOptions.basePath)
									: path.isAbsolute(pathOrPattern) && !/[?*+!@()[\]{}]/.test(pathOrPattern)
										? path.dirname(pathOrPattern)
										: path.resolve(path.dirname(id), path.dirname(pathOrPattern));

								let embeddedContent: string;

								const cacheKey = `${searchBasePath}\0${pathOrPattern}\0${callOptions.encoding}`;
								if (globCache.has(cacheKey)) {
									embeddedContent = globCache.get(cacheKey)!;
								} else {
									const isPotentialPattern = /[?*+!@()[\]{}]/.test(pathOrPattern);

									if (
										!isPotentialPattern &&
										fs.existsSync(path.resolve(searchBasePath, pathOrPattern)) &&
										fs.statSync(path.resolve(searchBasePath, pathOrPattern)).isFile()
									) {
										const singleFilePath = path.resolve(searchBasePath, pathOrPattern);

										try {
											const rawContent: string | Buffer = fs.readFileSync(singleFilePath, callOptions.encoding);
											const contentString = rawContent.toString();
											const fileInfo: FileInfo = {
												content: contentString,
												filePath: singleFilePath,
												fileName: path.relative(searchBasePath, singleFilePath),
											};
											embeddedContent = JSON.stringify(fileInfo);
											this.addWatchFile(singleFilePath);
										} catch (fileError: unknown) {
											let message = String(fileError instanceof Error ? fileError.message : (fileError ?? 'Unknown file read error'));
											this.error(`Error reading file ${singleFilePath}: ${message}`, node.loc?.start.index);
											return;
										}
									} else {
										const matchingFiles = glob.sync(pathOrPattern, {
											cwd: searchBasePath,
											nodir: true,
											absolute: true,
										});

										const fileInfoArray: FileInfo[] = [];
										for (const fullPath of matchingFiles) {
											try {
												const rawContent: string | Buffer = fs.readFileSync(fullPath, callOptions.encoding);
												const contentString = rawContent.toString();
												fileInfoArray.push({
													content: contentString,
													filePath: fullPath,
													fileName: path.relative(searchBasePath, fullPath),
												});
												this.addWatchFile(fullPath);
											} catch (fileError: unknown) {
												let message = String(fileError instanceof Error ? fileError.message : (fileError ?? 'Unknown file read error'));
												this.warn(`Error reading file ${fullPath}: ${message}`);
											}
										}
										embeddedContent = JSON.stringify(fileInfoArray);
									}
									globCache.set(cacheKey, embeddedContent);
								} // end cache miss

								// Replace the call expression with the generated content string
								magicString.overwrite(node.start, node.end, embeddedContent);
								hasReplaced = true;
								count++;
							} catch (error: unknown) {
								const message = String(error instanceof Error ? error.message : (error ?? 'Unknown error during file processing'));
								this.error(`Could not process files for constSysfsExpr: ${message}`, node.loc?.start.index);
								return;
							}
						}
					},
				});
			} catch (error: unknown) {
				const message = String(error instanceof Error ? error.message : (error ?? 'Unknown parsing error'));
				this.error(`Failed to parse ${id}: ${message}`);
				return null;
			}

			if (constSysfsImport !== null && hasReplaced) {
				const info = constSysfsImport as ImportSpecifierInfo;
				if (info.isOnlySpecifier) {
					let endPos = info.declEnd;
					if (code[endPos] === '\n') endPos++;
					else if (code[endPos] === '\r' && code[endPos + 1] === '\n') endPos += 2;
					magicString.remove(info.declStart, endPos);
				} else if (info.nextSpecifierStart !== null) {
					magicString.remove(info.specifierStart, info.nextSpecifierStart);
				} else if (info.prevSpecifierEnd !== null) {
					magicString.remove(info.prevSpecifierEnd, info.specifierEnd);
				}
			}

			// If no replacements were made, return null
			if (!hasReplaced) {
				return null;
			}

			// Return the modified code and source map
			const result: SourceDescription = {
				code: magicString.toString(),
				map: magicString.generateMap({ hires: true }),
			};
			return result;
		},
	};

	return { plugin, getCount: () => count };
}
