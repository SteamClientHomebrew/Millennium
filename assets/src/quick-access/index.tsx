import { ErrorBoundary, getParentWindow } from '@steambrew/client';
import { Component, createRef } from 'react';
import { PyFindAllPlugins } from '../utils/ffi';
import { PluginComponent } from '../types';
import { TitleView } from '../components/QuickAccessTitle';
import { PluginSelectorView, RenderPluginView } from './PluginView';
import { getActivePlugin, getDesktopMenuOpen, subscribeActivePlugin, subscribeDesktopMenu, setDesktopMenuOpen, setActivePlugin } from './desktopMenuStore';
import { BrowserManagerHook } from './browserHook';
import { MillenniumDesktopSidebarStyles } from '../utils/styles';

interface MillenniumDesktopSidebarState {
	closed: boolean;
	openAnimStart: boolean;
	plugins?: PluginComponent[];
	desktopMenuOpen: boolean;
	activePlugin?: PluginComponent;
}

export class MillenniumDesktopSidebar extends Component<{}, MillenniumDesktopSidebarState> {
	lastKeydownTime = 0;
	ref = createRef<HTMLDivElement>();
	closedInterval: ReturnType<typeof setTimeout> | null = null;
	unsubscribes: (() => void)[] = [];
	animFrame: number = 0;
	browserManagerHook = new BrowserManagerHook();

	constructor(props: {}) {
		super(props);

		const desktopMenuOpen = getDesktopMenuOpen();
		const activePlugin = getActivePlugin();

		this.state = {
			closed: !desktopMenuOpen,
			openAnimStart: desktopMenuOpen,
			plugins: undefined,
			desktopMenuOpen,
			activePlugin,
		};
	}

	handleKeyDown = (e: KeyboardEvent) => {
		if (!(e.ctrlKey && e.key === '2')) {
			return;
		}

		const now = Date.now();
		if (now - this.lastKeydownTime < 500) return;
		this.lastKeydownTime = now;

		e.preventDefault();

		this.setState({ desktopMenuOpen: !this.state.desktopMenuOpen });
		this.handleMenuToggle(!this.state.desktopMenuOpen);
	};

	componentDidMount() {
		this.getHostWindow()?.document?.addEventListener?.('keydown', this.handleKeyDown, true);
		this.browserManagerHook.hook();

		PyFindAllPlugins().then((pluginsJson: string) => this.setState({ plugins: JSON.parse(pluginsJson) }));

		const unsubscribeFromDesktopMenu = subscribeDesktopMenu((open) => {
			this.setState({ desktopMenuOpen: open });
			this.handleMenuToggle(open);
		});

		const unsubscribeFromActivePlugin = subscribeActivePlugin((plugin) => {
			this.setState({ activePlugin: plugin });
		});

		this.unsubscribes.push(unsubscribeFromDesktopMenu, unsubscribeFromActivePlugin);

		this.setAnimStart(getDesktopMenuOpen());
	}

	componentWillUnmount() {
		this.getHostWindow()?.document?.removeEventListener?.('keydown', this.handleKeyDown, true);

		if (this.closedInterval) clearTimeout(this.closedInterval);
		cancelAnimationFrame(this.animFrame);
		this.unsubscribes.forEach((unsubscribeCallback) => unsubscribeCallback());

		this.browserManagerHook.unhook();
	}

	setAnimStart = (value: boolean) => {
		this.setState({ openAnimStart: value });
	};

	getHostWindow() {
		return getParentWindow(this.ref.current);
	}

	closeQuickAccess = async () => {
		const hostWindow = this.getHostWindow();

		this.closedInterval = setTimeout(() => {
			this.setState({ closed: true });
			this.browserManagerHook.setBrowserVisible(hostWindow, true);
		}, 500);
	};

	openQuickAccess = async () => {
		const hostWindow = this.getHostWindow();

		setActivePlugin(null); // Reset active plugin when menu opens
		await this.browserManagerHook.setBrowserVisible(hostWindow, false);
		this.setState({ closed: false });
	};

	handleMenuToggle = async (desktopMenuOpen: boolean) => {
		this.browserManagerHook.setShouldBlockRequest(desktopMenuOpen);
		if (this.closedInterval) clearTimeout(this.closedInterval);

		desktopMenuOpen ? await this.openQuickAccess() : await this.closeQuickAccess();

		cancelAnimationFrame(this.animFrame);
		this.animFrame = requestAnimationFrame(() => {
			this.setAnimStart(desktopMenuOpen);
		});
	};

	handleSidebarDimClick = () => {
		setDesktopMenuOpen(false);
	};

	render() {
		const { closed, openAnimStart, plugins, desktopMenuOpen, activePlugin } = this.state;

		return (
			<ErrorBoundary>
				<MillenniumDesktopSidebarStyles isDesktopMenuOpen={desktopMenuOpen || !closed} openAnimStart={openAnimStart} isViewingPlugin={!!activePlugin} />
				<div className="MillenniumDesktopSidebarDim" onClick={this.handleSidebarDimClick} />
				<div className="MillenniumDesktopSidebar" ref={this.ref}>
					<TitleView />
					<div className="MillenniumDesktopSidebarContent">{activePlugin ? <RenderPluginView /> : <PluginSelectorView plugins={plugins} />}</div>
				</div>
			</ErrorBoundary>
		);
	}
}
