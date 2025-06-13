import { ErrorBoundary, getParentWindow } from '@steambrew/client';
import { useCallback, useEffect, useRef, useState } from 'react';
import { TitleView } from '../components/QuickAccessTitle';
import { PluginSelectorView, RenderPluginView } from './PluginView';
import { BrowserManagerHook } from './browserHook';
import { MillenniumDesktopSidebarStyles } from '../utils/styles';
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
			<MillenniumDesktopSidebarStyles isDesktopMenuOpen={isOpen || !closed} openAnimStart={openAnimStart} isViewingPlugin={!!activePlugin} />
			<div className="MillenniumDesktopSidebarDim" onClick={handleSidebarDimClick} />
			<div className="MillenniumDesktopSidebar" ref={ref}>
				<TitleView />
				<div className="MillenniumDesktopSidebarContent">{activePlugin ? <RenderPluginView /> : <PluginSelectorView />}</div>
			</div>
		</ErrorBoundary>
	);
};
