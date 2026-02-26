import { EUIComposition } from "./shared";

/**
 * Represents functionality for managing Steam's windows.
 *
 * "Restore details" here refers to a string that is similiar to
 * `1&x=604&y=257&w=1010&h=600`, which is usable with certain
 * `window.open()` parameters and methods from here.
 *
 * Note that methods here have to be called from the
 * window you want to use (not SharedJSContext).
 */
export interface Window {
    BringToFront(forceOS?: EWindowBringToFront): void;

    /**
     * Closes the window.
     */
    Close(): void;

    /**
     * @returns the window's fullscreen state.
     */
    DefaultMonitorHasFullscreenWindow(): Promise<boolean>;

    /**
     * Flashes the window in the taskbar.
     */
    FlashWindow(): void;

    GetDefaultMonitorDimensions(): Promise<MonitorDimensions>;

    /**
     * @returns the mouse position's restore details.
     */
    GetMousePositionDetails(): Promise<string>;

    /**
     * @returns the window's details.
     */
    GetWindowDetails(): Promise<WindowDetails>;

    /**
     * @returns the window's dimensions.
     */
    GetWindowDimensions(): Promise<WindowDimensions>;

    /**
     * @returns the window's restore details.
     */
    GetWindowRestoreDetails(): Promise<string>;

    /**
     * Hides the window.
     */
    HideWindow(): void;

    /**
     * @returns the window's maximized state.
     */
    IsWindowMaximized(): Promise<boolean>;

    /**
     * @returns the window's minimized state.
     */
    IsWindowMinimized(): Promise<boolean>;

    MarkLastFocused(): void;

    /**
     * Minimizes the window.
     */
    Minimize(): void;

    /**
     * Moves the window to given coordinates.
     * @param x Window X position.
     * @param y Window Y position.
     * @param dpi Screen DPI.
     */
    MoveTo(x: number, y: number, dpi?: number): void;

    /**
     * Moves the window to a given location.
     * @param location Window location.
     * @param offset X/Y offset.
     */
    MoveToLocation(location: WindowLocation_t, offset?: number): void;

    /**
     * Moves the window relatively to given details.
     * @param details Window restore details string from {@link GetWindowRestoreDetails}.
     * @param x Window X position.
     * @param y Window Y position.
     * @param width Window width.
     * @param height Window height.
     *
     * @example
     * Move the window to bottom right by 50 pixels:
     * ```
     * SteamClient.Window.GetWindowRestoreDetails((e) => {
     *     SteamClient.Window.PositionWindowRelative(e, 50, 50, 0, 0);
     * });
     * ```
     */
    PositionWindowRelative(details: string, x: number, y: number, width: number, height: number): void;

    /**
     * @returns `true` if yje naun [tpcess od about to shut down.]
     */
    ProcessShuttingDown(): Promise<boolean>;

    /**
     * Resizes the window to given dimension.
     * The window has to be created with the resizable flag.
     * @param width Window width.
     * @param height Window height.
     * @param applyBrowserScaleOrDPIValue
     */
    ResizeTo(width: number, height: number, applyBrowserScaleOrDPIValue: boolean | number): void;

    /**
     * Moves the window to given details.
     * @param details Window details string from `Window.GetWindowRestoreDetails`.
     */
    RestoreWindowSizeAndPosition(details: string): void;

    SetAutoDisplayScale(value: boolean): void;

    SetComposition(mode: EUIComposition, appIdCompositionQueue: number[], windowId: number): void;

    /**
     * Makes the window hide, but not close on pressing the close button.
     * @param value Hide on close?
     */
    SetHideOnClose(value: boolean): void;

    SetKeyFocus(value: boolean): void;

    SetManualDisplayScaleFactor(displayScaleFactor: number): void;

    /**
     * Sets the window's max size.
     * @param width Window's max width.
     * @param height Window's max height.
     */
    SetMaxSize(width: number, height: number): void;

    /**
     * Sets the window's min size.
     * @param width Window's max width.
     * @param height Window's max height.
     */
    SetMinSize(width: number, height: number): void;

    SetModal(value: boolean): void;

    /**
     * Sets the window's resize grip size.
     * The window has to be created with the resizable flag for this to take any effect.
     * @param width Resize grip width.
     * @param height Resize grip height.
     */
    SetResizeGrip(width: number, height: number): void;

    /**
     * Set the window's icon.
     * @param icon The window icon to be used.
     */
    SetWindowIcon(icon: WindowIcon_t): void;

    /**
     * Shows the window.
     */
    ShowWindow(): void;

    /**
     * Stops the window's taskbar flashing.
     */
    StopFlashWindow(): void;

    /**
     * Toggles the window's fullscreen state.
     */
    ToggleFullscreen(): void;

    /**
     * Toggles the window's maximized state.
     */
    ToggleMaximize(): void;
}

export enum EWindowBringToFront {
    Invalid,
    AndForceOS,
    WithoutForcingOS,
}

export type WindowLocation_t =
    | 'upper-left'
    | 'lower-left'
    | 'center-top'
    | 'center-bottom'
    | 'upper-right'
    | 'lower-right';

export type WindowIcon_t = 'steam' | 'messages' | 'voice';

/**
 * "Usable" here refers to space that is not taken by the taskbar.
 */
export interface MonitorDimensions {
    flHorizontalScale: number;
    flVerticalScale: number;
    nFullHeight: number;
    nFullLeft: number;
    nFullTop: number;
    nFullWidth: number;
    nUsableHeight: number;
    nUsableLeft: number;
    nUsableTop: number;
    nUsableWidth: number;
}

export interface WindowDetails {
    bGPUEnabled: boolean;
    bUnderlaySupported: boolean;
}

export interface WindowDimensions {
    x: number;
    y: number;
    width: number;
    height: number;
}
