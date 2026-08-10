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

import { afterPatch, findInReactTree, getReactInstance, getReactRoot } from '@steambrew/sdk';
import { GetSettingsMenuItems, RenderSettingsModal } from '../settings';

const ROOT_MENU_ITEM_ATTRIBUTE = 'data-millennium-root-menu-item';

function settingsItemsAreRendered(renderedItems: HTMLElement[]) {
	const renderedItemNames = new Set(renderedItems.map((item) => item.textContent));
	return GetSettingsMenuItems().every((item) => {
		return item === 'separator' || !item.visible || renderedItemNames.has(item.name);
	});
}

function removeRenderedRootMenuFallback(document: Document) {
	document.querySelectorAll(`[${ROOT_MENU_ITEM_ATTRIBUTE}]`).forEach((item) => item.remove());
}

function getRootMenuReactTree(document: Document) {
	const legacyRoot = document.getElementById('root');
	const legacyReactRoot = legacyRoot ? getReactRoot(legacyRoot) : undefined;
	if (legacyReactRoot) return legacyReactRoot;

	/** Steam beta mounts popup React trees below #popup_target instead of #root. */
	const popupRoot = document.querySelector('#popup_target > *');
	let popupFiber = popupRoot ? getReactInstance(popupRoot) : undefined;
	while (popupFiber?.return) popupFiber = popupFiber.return;
	return popupFiber;
}

function patchRenderedRootMenu(document: Document) {
	if (document.querySelector(`[${ROOT_MENU_ITEM_ATTRIBUTE}]`)) return;

	const renderedItems = Array.from(document.querySelectorAll<HTMLElement>('[role="menuitem"]'));
	if (settingsItemsAreRendered(renderedItems)) return;

	const exitItem = renderedItems.at(-1);
	const menuContainer = exitItem?.parentElement;
	if (!exitItem || !menuContainer) return;

	const separatorTemplate = exitItem.previousElementSibling;
	for (const item of GetSettingsMenuItems()) {
		if (item === 'separator') {
			if (separatorTemplate?.tagName === 'HR') {
				const separator = separatorTemplate.cloneNode(false) as HTMLElement;
				separator.setAttribute(ROOT_MENU_ITEM_ATTRIBUTE, 'separator');
				menuContainer.insertBefore(separator, exitItem);
			}
			continue;
		}
		if (!item.visible) continue;

		const menuItem = exitItem.cloneNode(false) as HTMLElement;
		menuItem.textContent = item.name;
		menuItem.setAttribute(ROOT_MENU_ITEM_ATTRIBUTE, item.name);
		menuItem.addEventListener('click', item.onClick);
		menuContainer.insertBefore(menuItem, exitItem);
	}

	/** Drop the fallback if a later Steam render applies the React patch. */
	const observer = new MutationObserver(() => {
		const reactItems = Array.from(
			document.querySelectorAll<HTMLElement>(`[role="menuitem"]:not([${ROOT_MENU_ITEM_ATTRIBUTE}])`),
		);
		if (!settingsItemsAreRendered(reactItems)) return;
		removeRenderedRootMenuFallback(document);
		observer.disconnect();
	});
	observer.observe(document.body, { childList: true, subtree: true });
}

export async function PatchRootMenu(document: Document) {
	/** The popup document may be reported just before React mounts its first child. */
	for (let attempt = 0; attempt < 20; attempt++) {
		const steamRootMenu = findInReactTree(getRootMenuReactTree(document), (m) => {
			return m?.pendingProps?.title === 'Steam' && m?.pendingProps?.menuContent;
		});

		if (steamRootMenu?.pendingProps?.menuContent) {
			afterPatch(steamRootMenu.pendingProps.menuContent, 'type', RenderSettingsModal);
			await new Promise((resolve) => setTimeout(resolve, 100));
			patchRenderedRootMenu(document);
			return;
		}

		await new Promise((resolve) => setTimeout(resolve, 100));
	}
}
