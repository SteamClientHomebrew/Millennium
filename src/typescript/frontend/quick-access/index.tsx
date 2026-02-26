/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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

import { ErrorBoundary, getParentWindow, IconsModule } from '@steambrew/client';
import { useCallback, useEffect, useRef, useState } from 'react';
import { TitleView } from '../components/QuickAccessTitle';
import { PluginSelectorView, RenderPluginView } from './PluginView';
import { BrowserManagerHook } from './browserHook';
import Styles, { MillenniumDesktopSidebarStyles } from '../utils/styles';
import { DesktopSideBarFocusedItemType, useDesktopMenu } from './DesktopMenuContext';
import { useQuickAccessStore } from './quickAccessStore';
import { RenderThemeView, ThemeSelectorView } from './ThemeView';
import { Placeholder } from '../components/Placeholder';
import { MillenniumIcons } from '../components/Icons';
import { locale } from '../utils/localization-manager';

export const MillenniumDesktopSidebar: React.FC = () => {
	const { isOpen, focusedItem, focusedItemType, closeMenu, openMenu, toggleMenu, setFocusedItem, plugins, themes } = useDesktopMenu();

	const [openAnimStart, setOpenAnimStartState] = useState(false);
	const [closed, setClosed] = useState(true);
	const ref = useRef<HTMLDivElement>(null);
	const closedInterval = useRef<ReturnType<typeof setTimeout> | null>(null);
	const animFrame = useRef<number>(0);
	const browserManagerHook = useRef(new BrowserManagerHook());

	const getHostWindow = useCallback(() => {
		return getParentWindow(ref.current);
	}, []);

	const setAnimStart = useCallback((value: boolean) => {
		setOpenAnimStartState(value);
	}, []);

	useEffect(() => {
		const unsubscribeOpen = useQuickAccessStore.getState().subscribeToOpen((userdata?: any) => {
			openMenu(userdata);
			openQuickAccess();
		});

		const unsubscribeClose = useQuickAccessStore.getState().subscribeToClose(() => {
			closeMenu();
			closeQuickAccess();
		});

		return () => {
			unsubscribeOpen();
			unsubscribeClose();
		};
	}, []);

	const closeQuickAccess = useCallback(async () => {
		const hostWindow = getHostWindow();

		if (closedInterval.current) {
			clearTimeout(closedInterval.current);
			closedInterval.current = null;
		}

		cancelAnimationFrame(animFrame.current);
		animFrame.current = requestAnimationFrame(() => {
			setAnimStart(false);
		});

		closedInterval.current = setTimeout(() => {
			setClosed(true);
			browserManagerHook.current.setBrowserVisible(hostWindow, true);
			closedInterval.current = null;
			setFocusedItem(undefined, undefined);
		}, 300);

		browserManagerHook.current.setShouldBlockRequest(false);
	}, [getHostWindow, setAnimStart]);

	const openQuickAccess = useCallback(async () => {
		browserManagerHook.current.setShouldBlockRequest(true);
		const hostWindow = getHostWindow();

		if (closedInterval.current) {
			clearTimeout(closedInterval.current);
			closedInterval.current = null;
		}

		try {
			await browserManagerHook.current.setBrowserVisible(hostWindow, false);
		} catch (error) {
			console.error('Error setting browser visibility:', error);
		}

		setClosed(false);

		setTimeout(() => {
			cancelAnimationFrame(animFrame.current);
			animFrame.current = requestAnimationFrame(() => {
				setAnimStart(true);
			});
		}, 8);
	}, [getHostWindow, setAnimStart]);

	const handleKeyDown = useCallback(
		(e: KeyboardEvent) => {
			if (!(e.ctrlKey && e.key === '2')) {
				return;
			}

			e.preventDefault();
			e.stopPropagation();

			toggleMenu();
		},
		[toggleMenu, isOpen],
	);

	const handleSidebarDimClick = useCallback(() => {
		closeMenu();
	}, [closeMenu]);

	useEffect(() => {
		if (isOpen && closed) {
			openQuickAccess();
		} else if (!isOpen && !closed) {
			closeQuickAccess();
		}
	}, [isOpen, closed, openQuickAccess, closeQuickAccess]);

	useEffect(() => {
		const hostWindow = getHostWindow();
		hostWindow?.document?.addEventListener?.('keydown', handleKeyDown, true);
		browserManagerHook.current.hook();

		return () => {
			hostWindow?.document?.removeEventListener?.('keydown', handleKeyDown, true);
			if (closedInterval.current) {
				clearTimeout(closedInterval.current);
				closedInterval.current = null;
			}
			cancelAnimationFrame(animFrame.current);
			try {
				browserManagerHook.current.unhook();
			} catch (error) {
				console.error('Error during cleanup:', error);
			}
		};
	}, []);

	function RenderByType() {
		const { plugins, themes } = useDesktopMenu();

		const hasConfigurableThemes = themes?.filter?.((theme) => theme?.data?.Conditions || theme?.data?.RootColors)?.length > 0;
		const hasConfigurablePlugins = plugins?.length > 0;

		if (!hasConfigurableThemes && !hasConfigurablePlugins) {
			return <Placeholder icon={<MillenniumIcons.Empty />} header={locale.quickAccessEmptyLibrary} body={locale.quickAccessEmptyLibraryBody} />;
		}

		if (!focusedItem) {
			return (
				<>
					<PluginSelectorView />
					<ThemeSelectorView />
				</>
			);
		}

		switch (focusedItemType) {
			case DesktopSideBarFocusedItemType.PLUGIN:
				return <RenderPluginView />;
			case DesktopSideBarFocusedItemType.THEME:
				return <RenderThemeView />;
			default:
				return null;
		}
	}

	return (
		<ErrorBoundary>
			<Styles />
			<MillenniumDesktopSidebarStyles isDesktopMenuOpen={isOpen || !closed} openAnimStart={openAnimStart} isViewingPlugin={!!focusedItem} />
			<div className="MillenniumDesktopSidebar_Overlay" onClick={handleSidebarDimClick} />
			<div className="MillenniumDesktopSidebar" ref={ref} data-focused-item-type={DesktopSideBarFocusedItemType[focusedItemType]}>
				<TitleView />
				<div className="MillenniumDesktopSidebar_Content">
					<RenderByType />
				</div>
			</div>
		</ErrorBoundary>
	);
};
