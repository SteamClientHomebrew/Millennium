import { ErrorBoundary, getParentWindow } from '@steambrew/client';
import { Component, createRef } from 'react';
import { PyFindAllPlugins } from '../utils/ffi';
import { PluginComponent } from '../types';
import { TitleView } from '../components/QuickAccessTitle';
import { QuickAccessVisibleState } from './QuickAccessContext';
import { PluginSelectorView, RenderPluginView } from './PluginView';
import { getActivePlugin, getDesktopMenuOpen, subscribeActivePlugin, subscribeDesktopMenu, setDesktopMenuOpen } from './desktopMenuStore';
import { BrowserManagerHook } from './browserHook';

interface MillenniumDesktopSidebarState {
	closed: boolean;
	openAnimStart: boolean;
	plugins?: PluginComponent[];
	desktopMenuOpen: boolean;
	activePlugin?: PluginComponent;
}

export class MillenniumDesktopSidebar extends Component<{}, MillenniumDesktopSidebarState> {
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

	componentDidMount() {
		this.browserManagerHook.hook();

		PyFindAllPlugins().then((pluginsJson: string) => this.setState({ plugins: JSON.parse(pluginsJson) }));

		const unsubMenu = subscribeDesktopMenu((open) => {
			this.setState({ desktopMenuOpen: open });
			this.handleMenuToggle(open);
		});

		const unsubPlugin = subscribeActivePlugin((plugin) => {
			this.setState({ activePlugin: plugin });
		});

		this.unsubscribes.push(unsubMenu, unsubPlugin);

		this.setAnimStart(getDesktopMenuOpen());
	}

	componentWillUnmount() {
		if (this.closedInterval) clearTimeout(this.closedInterval);
		cancelAnimationFrame(this.animFrame);
		this.unsubscribes.forEach((unsub) => unsub());

		this.browserManagerHook.unhook();
	}

	setAnimStart = (value: boolean) => {
		this.setState({ openAnimStart: value });
	};

	getHostWindow() {
		return getParentWindow(this.ref.current);
	}

	handleMenuToggle = async (desktopMenuOpen: boolean) => {
		this.browserManagerHook.setShouldBlockRequest(desktopMenuOpen);

		if (this.closedInterval) clearTimeout(this.closedInterval);

		const hostWindow = this.getHostWindow();

		if (desktopMenuOpen) {
			await this.browserManagerHook.setBrowserVisible(hostWindow, false);
			this.setState({ closed: false });
		} else {
			this.closedInterval = setTimeout(() => {
				this.setState({ closed: true });
				this.browserManagerHook.setBrowserVisible(hostWindow, true);
			}, 500);
		}

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

		const style = `
    .iconContainer {
      width: 22px;
      height: 22px;
    }
    .title-area { 
      z-index: 999999 !important; 
    }
    .MillenniumDesktopSidebarDim {
      position: absolute;
      height: 100%;
      width: 100%;
      top: 0px;
      left: 0px;
      z-index: 998;
      background: rgba(0, 0, 0, 0.7);
      opacity: ${openAnimStart ? 1 : 0};
      display: ${desktopMenuOpen || !closed ? 'flex' : 'none'};
      transition: opacity 0.4s cubic-bezier(0.65, 0, 0.35, 1);
    }
    .MillenniumDesktopSidebar {
      position: absolute;
      height: 100%;
      width: 350px;
      top: 0px;
      right: 0px;
      z-index: 999;
      transition: transform 0.4s cubic-bezier(0.65, 0, 0.35, 1);
      transform: ${openAnimStart ? 'translateX(0px)' : 'translateX(366px)'};
      overflow-y: auto;
      display: ${desktopMenuOpen || !closed ? 'flex' : 'none'};
      flex-direction: column;
      background: #171d25;
    }
    `;

		return (
			<ErrorBoundary>
				<style>{style}</style>
				<div className="MillenniumDesktopSidebarDim" onClick={this.handleSidebarDimClick} />
				<div className="MillenniumDesktopSidebar" ref={this.ref}>
					<QuickAccessVisibleState.Provider value={desktopMenuOpen || !closed}>
						<TitleView />
						<div style={{ padding: activePlugin ? '16px 20px 0px 20px' : '16px 0 0 0' }}>
							{activePlugin ? <RenderPluginView /> : <PluginSelectorView plugins={plugins} />}
						</div>
					</QuickAccessVisibleState.Provider>
				</div>
			</ErrorBoundary>
		);
	}
}
