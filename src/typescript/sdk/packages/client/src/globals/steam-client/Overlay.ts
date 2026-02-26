import type { EBrowserType, ESteamRealm, EUIComposition, EUIMode, Unregisterable } from "./shared";

export interface Overlay {
    /**
     * Destroys the gamepad UI desktop configurator window if open.
     */
    DestroyGamePadUIDesktopConfiguratorWindow(): void;

    GetOverlayBrowserInfo(): Promise<OverlayBrowserInfo[]>;

    // steam://gamewebcallback
    HandleGameWebCallback(url: string): void;

    /**
     * @param protocol Something like {@link OverlayBrowserProtocols.strScheme}
     */
    HandleProtocolForOverlayBrowser(appId: number, protocol: string): void;

    /**
     * Registers a callback function to be called when an overlay is activated from an app.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForActivateOverlayRequests(callback: (request: ActivateOverlayRequest) => void): Unregisterable;

    /**
     * Registers a callback function to be called when a microtransaction authorization is requested.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForMicroTxnAuth(
        callback: (appId: number, microTxnId: string, realm: ESteamRealm, microTxnUrl: string) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when a microtransaction authorization is dismissed by the user in Steam's authorization page.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForMicroTxnAuthDismiss(callback: (appId: number, microTxnId: string) => void): Unregisterable;

    RegisterForNotificationPositionChanged(
        callback: (appId: number, position: ENotificationPosition, horizontalInset: number, verticalInset: number) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when an overlay is activated or closed.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForOverlayActivated(
        callback: (overlayProcessPid: number, appId: number, active: boolean, param3: boolean) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when the overlay browser protocols change.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForOverlayBrowserProtocols(
        callback: (browseProtocols: OverlayBrowserProtocols) => void,
    ): Unregisterable;

    /**
     * Registers **the** callback function to be called when the overlay browser information changes.
     * @param callback The callback function to be called when the overlay browser information changes.
     * @returns an object that can be used to unregister the callback.
     * @remarks Do Not Use, this will break the overlay unless you know what you are doing.
     */
    RegisterOverlayBrowserInfoChanged(callback: () => void): Unregisterable;

    SetOverlayState(appId: string, state: EUIComposition): void;
}

type OverlayRequestDialog_t =
	| 'achievements'
	| 'asyncnotificationsrequested'
	| 'chat'
	| 'community'
	| 'friendadd'
	| 'friendremove'
	| 'friendrequestaccept'
	| 'friendrequestignore'
	| 'friendremove'
	| 'jointrade'
	| 'leaderboards'
	| 'lobbyinvite'
	| 'lobbyinviteconnectstring'
	| 'officialgamegroup'
	| 'requestplaytime'
	| 'remoteplaytogether'
	| 'remoteplaytogetherinvite'
	| 'settings'
	| 'stats'
	| 'steamid'
	| 'store';

// EPosition
export enum ENotificationPosition {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
}

export interface ActivateOverlayRequest {
    /**
     * The app ID that just had an overlay request.
     */
    appid: number;

    /**
     * `true` if webpage, and so {@link strDialog} will start with `https://`.
     */
    bWebPage: boolean;

    eFlag: EOverlayToStoreFlag;

    eWebPageMode: EActivateGameOverlayToWebPageMode;

    /**
     * Steam64 ID.
     */
    steamidTarget: string;

    /**
     * Game invites string for `Friends.InviteUserToGame`.
     */
    strConnectString: string;

    /**
     * Web page URL if starts with `https://`, so cast the type to `string` if it is.
     */
    strDialog: OverlayRequestDialog_t;

    /**
     * App ID of the requesting game.
     */
    unRequestingAppID: number;
}

export interface OverlayBrowserInfo {
    appID: number;
    eBrowserType: EBrowserType;
    eUIMode: EUIMode;
    flDisplayScale?: number;
    gameID: string;
    nBrowserID: number;
    nScreenHeight: number;
    nScreenWidth: number;
    /**
     * The PID of the overlay process.
     */
    unPID: number;
}

export interface OverlayBrowserProtocols {
    unAppID: number;
    strScheme: string;
    bAdded: boolean;
}

export enum EActivateGameOverlayToWebPageMode {
    Default,
    Modal,
}

export enum EOverlayToStoreFlag {
    None,
    AddToCart,
    AddToCartAndShow,
}
