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

import { afterPatch, findInReactTree, getReactRoot } from '@steambrew/sdk';
import { RenderSettingsModal, addRootMenuItems } from '../settings';

const ROOT_MENU_WRAPPER = 'RootMenuContent';

let wrapperRegistered = false;

export function RegisterRootMenuWrapper(): boolean {
	const registry = (window as any).__millennium_registry;
	if (!registry?.register || !registry.available?.[ROOT_MENU_WRAPPER]) return false;

	registry.register(ROOT_MENU_WRAPPER, (original: (...args: any[]) => any, ...args: any[]) => {
		const ret = original(...args);
		addRootMenuItems(ret);
		return ret;
	});
	wrapperRegistered = true;
	return true;
}

export function PatchRootMenu() {
	if (wrapperRegistered) return;

	const steamRootMenu = findInReactTree(getReactRoot(document.getElementById('root') as any), (m) => {
		return m?.pendingProps?.title === 'Steam' && m?.pendingProps?.menuContent;
	});

	if (steamRootMenu?.pendingProps?.menuContent) {
		afterPatch(steamRootMenu.pendingProps.menuContent, 'type', RenderSettingsModal);
	}
}
