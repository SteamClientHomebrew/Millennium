import { EUIMode, Unregisterable } from "./shared";
import {EOSType} from "./system";

export interface UI {
    EnsureMainWindowCreated(): void;

    ExitBigPictureMode(): void;

    GetDesiredSteamUIWindows(): Promise<SteamWindow[]>;

    /**
     * Gets information about whether your OS will be unsupported in the future or not.
     */
    GetOSEndOfLifeInfo(): Promise<OSEndOfLifeInfo>;

    /**
     * Retrieves the current UI mode.
     * @returns the current UI mode.
     */
    GetUIMode(): Promise<EUIMode>;

    NotifyAppInitialized(): void;

    RegisterDesiredSteamUIWindowsChanged(callback: () => void): Unregisterable;

    /**
     * Registers a callback function to be called when a convar's value gets changed.
     *
     * Hard crashes if such a convar does not exist or if you can't set it.
     *
     * @param convar The ConVar to watch.
     * @param callback The callback function to be called.
     */
    RegisterForClientConVar(convar: string, callback: (value: string) => void): Unregisterable;

    /**
     * @todo param{0,1} are enums
     */
    RegisterForErrorCondition(callback: (param0: number, param1: number) => void): Unregisterable;

    RegisterForKioskModeResetSignal(callback: () => void): Unregisterable;

    /**
     * @todo This fires multiple times.
     */
    RegisterForStartupFinished(callback: () => void): Unregisterable;

    RegisterForUIModeChanged(callback: (mode: EUIMode) => void): Unregisterable;

    ResetErrorCondition(): void;

    /**
     * Sets the UI mode to the specified value.
     * @param mode The UI mode to set.
     */
    SetUIMode(mode: EUIMode): void;
}

export enum EWindowType {
    MainGamepadUI,
    OverlayGamepadUI,
    Keyboard,
    ControllerConfigurator,
    VR,
    MainDesktopUI,
    DesktopLogin,
    OverlayDesktopUI,
    SteamChinaReviewLauncher,
}

export interface OSEndOfLifeInfo {
    bOSWillBeUnsupported: boolean;
    osType: EOSType;
}

/**
 * The following might have more correct information:
 * https://github.com/SteamDatabase/SteamTracking/blob/master/Protobufs/webuimessages_sharedjscontext.proto
 */
export interface SteamWindow {
    appid: number;
    hwndParent: number;
    nBrowserID: number;
    strAppName: string;
    unID: number;
    unPID: number;
    windowType: EWindowType;
    x: number;
    y: number;
}
