import {ETouchGesture} from '../Browser';
import { BrowserView } from '.';

export interface BrowserViewPopup {
    /**
     * Blur the popup contents.
     * @param enabled Is the blur enabled?
     * @param useBackgroundColor
     * @param blur
     */
    AddGlass(enabled: boolean, useBackgroundColor: boolean, blur: boolean): void;

    /**
     * @returns `true` if you can go backward in history.
     */
    CanGoBackward(): boolean;

    /**
     * @returns `true` if you can go forward in history.
     */
    CanGoForward(): boolean;

    // alert() i assume
    DialogResponse(param0: boolean): void;

    EnableSteamInput(): void;

    /**
     * Find a string in the page.
     * @param input The string to find.
     * @param param1
     * @param previous `true` for previous match, `false` for next match.
     */
    FindInPage(input: string, param1: boolean, previous: boolean): void;

    /**
     * Get the current popup position. Only updates when using {@link SetBounds}!
     * @returns the window position.
     */
    GetBounds(): BrowserViewBounds;

    /**
     * Go back in history.
     */
    GoBack(): void;

    /**
     * Go forward in history.
     */
    GoForward(): void;

    /**
     * @param command See {@link BrowserViewContextMenu.custom_commands}.
     */
    HandleContextMenuCommand(command: number, param2: BrowserViewContextMenu): void;

    /**
     * Load the specified URL.
     * @param url The URL to go to.
     */
    LoadURL(url: string): void;

    NotifyUserActivation(): void;

    /**
     * Paste the current clipboard selection.
     */
    Paste(): void;

    /**
     * @returns a boolean indicating whether the operation was successful. Will
     * always be `false` if there are no listeners created by
     * {@link BrowserView.RegisterForMessageFromParent}.
     */
    PostMessage(message: string, args: string): boolean;

    /**
     * Reload the page.
     */
    Reload(): void;

    /**
     * Load the specified URL, but don't save history.
     * @param url The URL to go to.
     */
    ReplaceURL(url: string): void;

    /**
     * Define blocked protocols, like https, etc.
     * @param protocols The protocols to block, separated by a semicolon.
     */
    SetBlockedProtocols(protocols: string): void;

    /**
     * Sets the browser window position.
     * @param x Browser window X position.
     * @param y Browser window Y position.
     * @param width Browser window width.
     * @param height Browser window height.
     */
    SetBounds(x: number, y: number, width: number, height: number): void;

    /**
     * Sets the browser window focus state.
     * @param value Is the window focused?
     */
    SetFocus(value: boolean): void;

    SetName(browserName: string): void;

    /**
     * Registers a callback to be called when a context menu is shown.
     * @param callback The callback function to be called.
     */
    SetShowContextMenuCallback(callback: (data: BrowserViewContextMenu) => void): void;

    /**
     * Registers a callback to be called when a steam:// URL is loaded.
     */
    SetSteamURLCallback(callback: (url: string) => void): void;

    /**
     * @todo unconfirmed
     */
    SetTouchGesturesToCancel(gestures: ETouchGesture[]): void;

    SetVRKeyboardVisibility(value: boolean): void;

    SetVisible(value: boolean): void;

    SetWindowStackingOrder(value: EWindowStackingOrder): void;

    /**
     * Stop the 'find in page' function.
     */
    StopFindInPage(): void;

    /**
     * Stop listening for an event.
     * @param event The event to stop listening to.
     * @param callback The callback function to be called.
     */
    off<K extends keyof BrowserViewEventMap>(event: K, callback: BrowserViewEventMap[K]): void;

    /**
     * Start listening for an event.
     * @param event The event to start listening to.
     * @param callback The callback function to be called.
     */
    on<K extends keyof BrowserViewEventMap>(event: K, callback: BrowserViewEventMap[K]): void;
}

interface BrowserViewEventMap {
    /**
     * Fires when an `alert()` dialog appears.
     */
    'alert-dialog': (message: string) => void;

    /**
     * Fires when the browser is about to get destroyed.
     */
    'before-close': () => void;

    /**
     * Fires when a URL gets blocked.
     * @todo not SetBlockedProtocols, maybe only steam links
     */
    'blocked-request': (blockedURL: string) => void;

    /**
     * Fires when {@link BrowserViewPopup.CanGoBackward} or
     * {@link BrowserViewPopup.CanGoForward} state changes.
     */
    'can-go-back-forward-changed': (
        canGoBackward: boolean,
        canGoForward: boolean,
    ) => void;

    /**
     * Fires when a `confirm()` dialog appears.
     */
    'confirm-dialog': (message: string) => void;

    /**
     * Fires when the browser's favicon changes.
     */
    'favicon-urls-changed': (faviconURLs: string[]) => void;

    /**
     * Fires when 'Find in page' gets its results.
     */
    'find-in-page-results': (results: number, activeResultIndex: number) => void;

    /**
     * Fires when the page finishes loading.
     */
    'finished-request': (currentURL: string, previousURL: string) => void;

    /**
     * Fires when the browser goes focused or vice versa.
     */
    'focus-changed': (focused: boolean) => void;

    /**
     * Fires when the browser goes fullscreen or vice versa.
     */
    'full-screen': (fullscreen: boolean) => void;

    /**
     * Fires when history changes occur.
     */
    'history-changed': (history: BrowserViewHistory) => void;

    /**
     * Fires when the URL fails to load.
     */
    'load-error': (
        errorCode: number,
        errorURL: string,
        errorDescription: string,
    ) => void;

    /**
     * Fires when a message gets sent with {@link BrowserView.PostMessageToParent}.
     */
    'message': (message: string, args: string, currentURL: string) => void;

    'new-tab': (url: string, param1: boolean) => void;

    /**
     * Fires when a node gets focused.
     */
    'node-has-focus': (
        /**
         * Same as {@link Element.tagName}, but the value is the element's ID when exists.
         */
        elementIdOrTagName: string,
        /**
         * Same as {@link Element.tagName}.
         */
        elementTag: string,
        /**
         * The `type` attribute of an `<input>` element.
         */
        inputType: string,
        /**
         * @todo Localization token that doesn't seem to exist? (#Web_FormSubmit)
         */
        token: string,
        param4: boolean,
    ) => void;

    'page-security': (url: string, pageSecurity: BrowserViewPageSecurity) => void;

    /**
     * Fires when the page's `<title>` changes.
     */
    'set-title': (title: string) => void;

    /**
     * Fires when the page starts loading.
     */
    'start-loading': (url: string, param1: boolean) => void;

    /**
     * Fires when the page starts loading.
     */
    'start-request': (url: string) => void;

    /**
     * Fires when "Find in page" gets toggled.
     */
    'toggle-find-in-page': () => void;
}

export interface BrowserViewBounds {
    x: number;
    y: number;
    width: number;
    height: number;
}

export interface BrowserViewContextMenu {
    /**
     * Mouse X position inside the browser view.
     */
    coord_x: number;

    /**
     * Mouse Y position inside the browser view.
     */
    coord_y: number;

    custom_commands: BrowserViewContextMenuCustomCommand[];

    /**
     * Bitmask representing edit state. Appears on editable elements like `<input>`, etc.
     * @see {@link EBrowserViewContextMenuEditFlag}
     */
    edit_state_flags?: number;

    /**
     * The misspelled word the cursor is on. Appears on an editable element with text.
     */
    misspelled_word?: string;

    /**
     * Browser page URL.
     * @todo Appears when there is selected text?
     */
    link_url?: string;

    /**
     * Browser page URL.
     */
    page_url: string;

    /**
     * Selected text. Appears when there is selected text.
     */
    selection_text?: string;

    /**
     * Bitmask representing context menu type.
     * @see {@link EBrowserViewContextMenuTypeFlag}
     */
    type_flags: number;

    /**
     * Browser page URL.
     * @todo Appears when there is selected text?
     */
    unfiltered_link_url?: string;
}

export interface BrowserViewContextMenuCustomCommand {
    id: number;
    label: string;
}

export enum EBrowserViewContextMenuTypeFlag {
    None,
    Page = 1 << 0,
    Frame = 1 << 1,
    Link = 1 << 2,
    Media = 1 << 3,
    Selection = 1 << 4,
    Editable = 1 << 5,
}

export enum EBrowserViewContextMenuEditFlag {
    None,
    CanUndo = 1 << 0,
    CanRedo = 1 << 1,
    CanCut = 1 << 2,
    CanCopy = 1 << 3,
    CanPaste = 1 << 4,
    CanDelete = 1 << 5,
    CanSelectAll = 1 << 6,
    CanTranslate = 1 << 7,
}

export enum EBrowserViewContextMenuCommand {
    Close = -1,
    OpenDevTools = 26500,
    CloseDevTools,
    InspectElement,
    OpenLinkInNewTab,
}

export enum EWindowStackingOrder {
    Bottom,
    Top,
}

export interface BrowserViewHistory {
    index: number;
    entries: BrowserViewHistoryEntry[];
}

export interface BrowserViewHistoryEntry {
    url: string;
}

export interface BrowserViewPageSecurity {
    bHasCertError: boolean;
    bIsEVCert: boolean;
    bIsSecure: boolean;
    certExpiry: number;
    certName: string;
    issuerName: string;
    nCertBits: number;
}
