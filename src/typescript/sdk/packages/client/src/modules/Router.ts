import Logger from '../logger';
import { Export, findModuleExport } from '../webpack';
import { EDisplayStatus } from '../globals/steam-client/App';

export enum SideMenu {
	None,
	Main,
	QuickAccess,
}

export enum QuickAccessTab {
	Notifications,
	RemotePlayTogetherControls,
	VoiceChat,
	Friends,
	Settings,
	Perf,
	Help,
	Music,
	Decky = 999,
}

export type AppOverview = {
	appid: string;
	display_name: string;
	display_status: EDisplayStatus;
	sort_as: string;
};

export interface MenuStore {
	OpenSideMenu(sideMenu: SideMenu): void;
	OpenQuickAccessMenu(quickAccessTab?: QuickAccessTab): void;
	OpenMainMenu(): void;
}

export interface WindowRouter {
	BrowserWindow: Window;
	MenuStore: MenuStore;
	Navigate(path: string): void;
	NavigateToChat(): void;
	NavigateToSteamWeb(url: string): void;
	NavigateBack(): void;
}

export interface WindowStore {
	GamepadUIMainWindowInstance?: WindowRouter; // Current
	SteamUIWindows: WindowRouter[];
	OverlayWindows: WindowRouter[]; // Used by desktop GamepadUI
}

export interface Router {
	WindowStore?: WindowStore;
	/** @deprecated use {@link Navigation} instead */
	CloseSideMenus(): void;
	/** @deprecated use {@link Navigation} instead */
	Navigate(path: string): void;
	/** @deprecated use {@link Navigation} instead */
	NavigateToAppProperties(): void;
	/** @deprecated use {@link Navigation} instead */
	NavigateToExternalWeb(url: string): void;
	/** @deprecated use {@link Navigation} instead */
	NavigateToInvites(): void;
	/** @deprecated use {@link Navigation} instead */
	NavigateToChat(): void;
	/** @deprecated use {@link Navigation} instead */
	NavigateToLibraryTab(): void;
	/** @deprecated use {@link Navigation} instead */
	NavigateToLayoutPreview(e: unknown): void;
	/** @deprecated use {@link Navigation} instead */
	OpenPowerMenu(unknown?: any): void;
	get RunningApps(): AppOverview[];
	get MainRunningApp(): AppOverview | undefined;
}

export const Router = findModuleExport((e: Export) => e.Navigate && e.NavigationManager) as Router;

export interface Navigation {
	Navigate(path: string): void;
	NavigateBack(): void;
	NavigateToAppProperties(): void;
	NavigateToExternalWeb(url: string): void;
	NavigateToInvites(): void;
	NavigateToChat(): void;
	NavigateToLibraryTab(): void;
	NavigateToLayoutPreview(e: unknown): void;
	NavigateToSteamWeb(url: string): void;
	OpenSideMenu(sideMenu: SideMenu): void;
	OpenQuickAccessMenu(quickAccessTab?: QuickAccessTab): void;
	OpenMainMenu(): void;
	OpenPowerMenu(unknown?: any): void;
	/** if calling this to perform navigation, call it after Navigate to prevent a race condition in desktop Big Picture mode that hides the overlay unintentionally */
	CloseSideMenus(): void;
}

export let Navigation = {} as Navigation;

const logger = new Logger('Navigation');

try {
	function createNavigationFunction(fncName: string, handler?: (win: any) => any) {
		return (...args: any) => {
			let win: WindowRouter | undefined;
			try {
				win = window.SteamUIStore.GetFocusedWindowInstance();
			} catch (e) {
				logger.warn('Navigation interface failed to call GetFocusedWindowInstance', e);
			}
			if (!win) {
				logger.warn('Navigation interface could not find any focused window. Falling back to Main Window Instance');
				win =
					Router.WindowStore?.GamepadUIMainWindowInstance ||
					Router?.WindowStore?.SteamUIWindows?.find((window) => window?.BrowserWindow?.name === 'SP Desktop_uid0');
			}

			if (win) {
				try {
					const thisObj = handler && handler(win);
					(thisObj || win)[fncName](...args);
				} catch (e) {
					logger.error('Navigation handler failed', e);
				}
			} else {
				logger.error('Navigation interface could not find a window to navigate');
			}
		};
	}
	const newNavigation = {
		Navigate: createNavigationFunction('Navigate'),
		NavigateBack: createNavigationFunction('NavigateBack'),
		NavigateToAppProperties: createNavigationFunction('AppProperties', (win) => win.Navigator),
		NavigateToExternalWeb: createNavigationFunction('ExternalWeb', (win) => win.Navigator),
		NavigateToInvites: createNavigationFunction('Invites', (win) => win.Navigator),
		NavigateToChat: createNavigationFunction('Chat', (win) => win.Navigator),
		NavigateToLibraryTab: createNavigationFunction('LibraryTab', (win) => win.Navigator),
		NavigateToLayoutPreview: Router.NavigateToLayoutPreview?.bind(Router),
		NavigateToSteamWeb: createNavigationFunction('NavigateToSteamWeb'),
		OpenSideMenu: createNavigationFunction('OpenSideMenu', (win) => win.MenuStore),
		OpenQuickAccessMenu: createNavigationFunction('OpenQuickAccessMenu', (win) => win.MenuStore),
		OpenMainMenu: createNavigationFunction('OpenMainMenu', (win) => win.MenuStore),
		CloseSideMenus: createNavigationFunction('CloseSideMenus', (win) => win.MenuStore),
		OpenPowerMenu: Router.OpenPowerMenu?.bind(Router),
	} as Navigation;

	Object.assign(Navigation, newNavigation);
} catch (e) {
	logger.error('Error initializing Navigation interface', e);
}
