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

import { parse } from '@babel/parser';
import _traverse from '@babel/traverse';
import type { Plugin } from 'rollup';
import MagicString from 'magic-string';

const traverse = (typeof _traverse === 'function' ? _traverse : (_traverse as any).default) as typeof _traverse;

export interface RenameTransform {
	type: 'rename';
	match: string[];
	replacement: string;
}

export interface InjectArgTransform {
	type: 'inject_arg';
	match: string[];
	arg: string;
}

export interface InjectConstTransform {
	type: 'inject_const';
	match: string[];
	localName: string;
	init: string;
}

export type Transform = RenameTransform | InjectArgTransform | InjectConstTransform;

function getMemberPath(node: any): string[] | null {
	if (node.type === 'Identifier') return [node.name];
	if ((node.type === 'MemberExpression' || node.type === 'OptionalMemberExpression') && !node.computed) {
		const obj = getMemberPath(node.object);
		if (!obj) return null;
		if (node.property.type === 'Identifier') return [...obj, node.property.name];
	}
	return null;
}

function pathEquals(a: string[], b: string[]): boolean {
	return a.length === b.length && a.every((seg, i) => seg === b[i]);
}

function findTransform<T extends Transform>(transforms: Transform[], path: string[], type: T['type']): T | undefined {
	return transforms.find((t) => t.type === type && pathEquals(path, t.match)) as T | undefined;
}

export default function astTransforms(transforms: Transform[]): Plugin {
	const renames = transforms.filter((t): t is RenameTransform => t.type === 'rename');
	const injections = transforms.filter((t): t is InjectArgTransform => t.type === 'inject_arg');
	const injectConsts = transforms.filter((t): t is InjectConstTransform => t.type === 'inject_const');

	return {
		name: 'ast-transforms',
		renderChunk(code) {
			let ast: any;
			try {
				ast = parse(code, { sourceType: 'script', plugins: ['jsx'] });
			} catch {
				return null;
			}

			const magic = new MagicString(code);
			let modified = false;

			// transform → [{start, end}]
			const icMatches = new Map<InjectConstTransform, Array<{ start: number; end: number }>>();

			const handleCall = (nodePath: any) => {
				const callee = nodePath.node.callee;
				const memberPath = getMemberPath(callee);
				if (!memberPath) return;

				const injection = findTransform<InjectArgTransform>(injections, memberPath, 'inject_arg');
				if (injection) {
					const args = nodePath.node.arguments;

					if (args.length > 0 && args[0].type === 'Identifier' && (args[0] as any).name === injection.arg) {
						return;
					}

					const calleeEnd = (callee as any).end as number;
					let parenPos = calleeEnd;
					while (parenPos < code.length && code[parenPos] !== '(') parenPos++;

					const prefix = args.length > 0 ? `${injection.arg}, ` : injection.arg;
					magic.appendLeft(parenPos + 1, prefix);
					modified = true;
				}
			};

			traverse(ast, {
				CallExpression: handleCall,
				OptionalCallExpression: handleCall,
				MemberExpression(nodePath) {
					const memberPath = getMemberPath(nodePath.node);
					if (!memberPath) return;

					if (nodePath.parentPath?.isMemberExpression() && nodePath.parentPath.node.object === nodePath.node) {
						const parentProp = (nodePath.parentPath.node as any).property;
						if (parentProp?.type === 'Identifier') {
							const extendedPath = [...memberPath, parentProp.name];
							if (transforms.some((t) => pathEquals(t.match, extendedPath))) {
								return;
							}
						}
					}

					const start = (nodePath.node as any).start as number;
					const end = (nodePath.node as any).end as number;

					const rename = findTransform<RenameTransform>(renames, memberPath, 'rename');
					if (rename) {
						magic.overwrite(start, end, rename.replacement);
						modified = true;
						return;
					}

					const ic = findTransform<InjectConstTransform>(injectConsts, memberPath, 'inject_const');
					if (ic) {
						if (!icMatches.has(ic)) icMatches.set(ic, []);
						icMatches.get(ic)!.push({ start, end });
					}
				},
			});

			if (icMatches.size > 0) {
				let iifeBodyStart = -1;
				traverse(ast, {
					FunctionExpression(nodePath) {
						if (nodePath.parentPath?.isCallExpression()) {
							const body = nodePath.node.body;
							iifeBodyStart = body.body.length > 0 ? (body.body[0] as any).start : (body as any).end - 1;
							nodePath.stop();
						}
					},
				});

				for (const [ic, positions] of icMatches) {
					if (iifeBodyStart === -1) continue;
					for (const { start, end } of [...positions].sort((a, b) => b.start - a.start)) {
						magic.overwrite(start, end, ic.localName);
					}
					magic.prependLeft(iifeBodyStart, `const ${ic.localName} = ${ic.init};\n`);
					modified = true;
				}
			}

			if (!modified) return null;
			return { code: magic.toString(), map: magic.generateMap({ hires: true }) };
		},
	};
}
