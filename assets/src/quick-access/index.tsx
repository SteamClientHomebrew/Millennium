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

import { ErrorBoundary, getParentWindow } from '@steambrew/client';
import { useCallback, useEffect, useRef, useState } from 'react';
import { TitleView } from '../components/QuickAccessTitle';
import { PluginSelectorView, RenderPluginView } from './PluginView';
import { BrowserManagerHook } from './browserHook';
import Styles, { MillenniumDesktopSidebarStyles } from '../utils/styles';
import { useDesktopMenu } from './DesktopMenuContext';
import { useQuickAccessStore } from './quickAccessStore';

export const MillenniumDesktopSidebar: React.FC = () => {
	const { isOpen, activePlugin, closeMenu, openMenu, toggleMenu, setActivePlugin } = useDesktopMenu();

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
		const unsubscribeOpen = useQuickAccessStore.getState().subscribeToOpen(() => {
			openMenu();
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
			setActivePlugin(undefined);
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

	return (
		<ErrorBoundary>
			<Styles />
			<MillenniumDesktopSidebarStyles isDesktopMenuOpen={isOpen || !closed} openAnimStart={openAnimStart} isViewingPlugin={!!activePlugin} />
			<div className="MillenniumDesktopSidebar_Overlay" onClick={handleSidebarDimClick} />
			<div className="MillenniumDesktopSidebar" ref={ref}>
				<TitleView />
				<div className="MillenniumDesktopSidebar_Content">{activePlugin ? <RenderPluginView /> : <PluginSelectorView />}</div>
			</div>
		</ErrorBoundary>
	);
};
