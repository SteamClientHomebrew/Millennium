import { JsPbMessage, OperationResponse, Unregisterable } from "../shared";
import {Audio} from "./Audio";
import {AudioDevice} from "./AudioDevice";
import {Bluetooth} from "./Bluetooth";
import {Devkit} from "./Devkit";
import {Display} from "./Display";
import {DisplayManager} from "./DisplayManager";
import {Dock} from "./Dock";
import {Network} from "./network";
import {Perf} from "./Perf";
import {Report} from "./Report";
import {UI} from "./UI";

export interface System {
    Audio: Audio;
    AudioDevice: AudioDevice;
    Bluetooth: Bluetooth;

    /**
     * @returns a boolean indicating whether the operation succeeded.
     */
    CopyFile(target: string, destination: string): Promise<boolean>;

    /**
     * Copies specified files to clipboard.
     * Does not throw if not found.
     * @param paths File paths to copy.
     */
    CopyFilesToClipboard(paths: string[]): void;

    /**
     * Creates a temporary folder.
     * @param path The folder to create.
     * @returns the created path.
     * @todo Does this support relative paths ? this has some weird behavior
     */
    CreateTempPath(path: string): Promise<string>;

    Devkit: Devkit;
    Display: Display;
    DisplayManager: DisplayManager;
    Dock: Dock;

    ExitFakeCaptivePortal(): any;

    FactoryReset(): any;

    FormatStorage(force: boolean): any;

    GetOSType(): Promise<EOSType>;

    GetSystemInfo(): Promise<SystemInfo>;

    IsDeckFactoryImage(): Promise<boolean>;

    IsSteamInTournamentMode(): Promise<boolean>;

    /**
     * Moves a file.
     * @param target Target file/folder.
     * @param destination Destination path.
     * @remarks Does not throw on error.
     */
    MoveFile(target: string, destination: string): void;

    Network: Network;

    NotifyGameOverlayStateChanged(latestAppOverlayStateActive: boolean, appId: number): any;

    /**
     * Open a dialog for choosing a file.
     * @param prefs Dialog preferences.
     * @returns the selected file name.
     * @throws OperationResponse if no file was selected.
     */
    OpenFileDialog(prefs: FileDialog): Promise<string | OperationResponse>;

    /**
     * Open a URL in the default web browser.
     */
    OpenInSystemBrowser(url: string): void;

    OpenLocalDirectoryInSystemExplorer(directory: string): void;
    Perf: Perf;

    RebootToAlternateSystemPartition(): any;

    RebootToFactoryTestImage(param0: any): any;

    RegisterForAirplaneModeChanges(callback: (state: AirplaneModeState) => void): Unregisterable;

    RegisterForBatteryStateChanges(callback: (state: BatteryState) => void): Unregisterable;

    RegisterForFormatStorageProgress(callback: (progress: FormatStorageProgress) => void): Unregisterable;

    RegisterForOnResumeFromSuspend(callback: () => void): Unregisterable;

    RegisterForOnSuspendRequest(callback: () => void): Unregisterable;

    /**
     * @returns a ProtoBuf message. If deserialized, returns {@link CMsgSystemManagerSettings}.
     */
    RegisterForSettingsChanges(callback: (data: ArrayBuffer) => void): Unregisterable;

    Report: Report;

    /**
     * Restarts the system.
     */
    RestartPC(): any;

    SetAirplaneMode(value: boolean): void;

    ShutdownPC(): any;

    SteamRuntimeSystemInfo(): Promise<string>;

    /**
     * Suspends the system.
     */
    SuspendPC(): any;

    /**
     * Switches to desktop mode.
     */
    SwitchToDesktop(): any;

    UI: UI;
    UpdateSettings: any;

    VideoRecordingDriverCheck(): any;
}

export interface AirplaneModeState {
    bEnabled: boolean;
}

export interface BatteryState {
    bHasBattery: boolean;
    eACState: EACState;
    eBatteryState: EBatteryState;
    /**
     * Battery Percentage in floating point 0-1.
     */
    flLevel: number;
    /**
     * Appears to be charge time remaining or time remaining on battery.
     */
    nSecondsRemaining: number;
    bShutdownRequested: boolean;
}

export interface FormatStorageProgress {
  flProgress: number;
  rtEstimatedCompletionTime: number;
  eStage: EStorageFormatStage;
}

export enum EACState {
    Unknown,
    Disconnected,
    Connected,
    ConnectedSlow,
}

export enum EBatteryState {
    Unknown,
    Discharging,
    Charging,
    Full,
}

export enum EStorageFormatStage {
  Invalid,
  NotRunning,
  Starting,
  Testing,
  Rescuing,
  Formatting,
  Finalizing,
}

export interface FileDialog {
    /** Whether to choose a directory instead. */
    bChooseDirectory?: boolean;
    /**
     * Array of file filters.
     * @example
     * Example from the "Add a Non-Steam Game" dialog:
     * ```
     * [
     *     {
     *         strFileTypeName: LocalizationManager.LocalizeString("#AddNonSteam_Filter_Exe_Linux"),
     *         rFilePatterns: [ "*.application", "*.exe", "*.sh", "*.AppImage" ],
     *         bUseAsDefault: true,
     *     },
     *     {
     *         strFileTypeName: LocalizationManager.LocalizeString("#AddNonSteam_Filter_All"),
     *         rFilePatterns: [ "*" ],
     *     }
     * ]
     * ```
     */
    rgFilters?: FileDialogFilter[];
    /** Initially selected file. */
    strInitialFile?: string;
    /** Window title. */
    strTitle?: string;
}

export interface FileDialogFilter {
    /** A localization string for the file type. */
    strFileTypeName: string;
    /**
     * File patterns.
     * @example [ "*.application", "*.exe", "*.sh", "*.AppImage" ]
     */
    rFilePatterns: string[];
    /** Whether to use this filter by default. */
    bUseAsDefault?: boolean;
}

export enum EOSType {
    Web = -700,
    Ios = -600,
    Android = -500,
    Android6 = -499,
    Android7 = -498,
    Android8 = -497,
    Android9 = -496,
    Ps3os = -300,
    Linux = -203,
    Linux22 = -202,
    Linux24 = -201,
    Linux26 = -200,
    Linux32 = -199,
    Linux35 = -198,
    Linux36 = -197,
    Linux310 = -196,
    Linux316 = -195,
    Linux318 = -194,
    Linux3x = -193,
    Linux4x = -192,
    Linux41 = -191,
    Linux44 = -190,
    Linux49 = -189,
    Linux414 = -188,
    Linux419 = -187,
    Linux5x = -186,
    Linux54 = -185,
    Linux6x = -184,
    Linux7x = -183,
    Linux510 = -182,
    Macos = -102,
    Macos104 = -101,
    Macos105 = -100,
    Macos1058 = -99,
    Macos106_unused1 = -98,
    Macos106_unused2 = -97,
    Macos106_unused3 = -96,
    Macos106 = -95,
    Macos1063 = -94,
    Macos1064_slgu = -93,
    Macos1067 = -92,
    Macos1067_unused = -91,
    Macos107 = -90,
    Macos108 = -89,
    Macos109 = -88,
    Macos1010 = -87,
    Macos1011 = -86,
    Macos1012 = -85,
    Macos1013 = -84,
    Macos1014 = -83,
    Macos1015 = -82,
    Macos1016 = -81,
    Macos11 = -80,
    Macos111 = -79,
    Macos1017 = -78,
    Macos12 = -77,
    Macos1018 = -76,
    Macos13 = -75,
    Macos1019 = -74,
    Macos14 = -73,
    Macos1020 = -72,
    Macos15 = -71,
    Unknown = -1,
    Windows = 0,
    Win311 = 1,
    Win95 = 2,
    Win98 = 3,
    WinME = 4,
    WinNT = 5,
    Win200 = 6,
    WinXP = 7,
    Win2003 = 8,
    WinVista = 9,
    Win7 = 10,
    Win2008 = 11,
    Win2012 = 12,
    Win8 = 13,
    Win81 = 14,
    Win2012R2 = 15,
    Win10 = 16,
    Win2016 = 17,
    Win2019 = 18,
    Win2022 = 19,
    Win11 = 20,
}

export interface SystemInfo {
    sOSName: string;
    sKernelVersion: string;
    sBIOSVersion: string;
    sHostname: string;
    sOSCodename: string;
    sOSVariantId: string;
    sOSVersionId: string;
    sOSBuildId: string;
    nSteamVersion: number;
    sSteamBuildDate: string;
    sSteamAPI: string;
    sCPUVendor: string;
    sCPUName: string;
    nCPUHz: number;
    nCPUPhysicalCores: number;
    nCPULogicalCores: number;
    nSystemRAMSizeMB: number;
    sVideoCardName: string;
    sVideoDriverVersion: string;
    nVideoRAMSizeMB: number;
    bIsUnsupportedPrototypeHardware: boolean;
}

export interface CMsgSystemManagerSettings extends JsPbMessage {
    display_adaptive_brightness_enabled(): boolean;

    display_colorgamut(): number;

    display_colorgamut_labelset(): number;

    display_colortemp(): number;

    display_colortemp_default(): number;

    display_colortemp_enabled(): boolean;

    display_diagnostics_enabled(): boolean;

    display_nightmode_blend(): number;

    display_nightmode_enabled(): boolean;

    display_nightmode_maxhue(): number;

    display_nightmode_maxsat(): number;

    display_nightmode_schedule_enabled(): boolean;

    display_nightmode_schedule_endtime(): number;

    display_nightmode_schedule_starttime(): number;

    display_nightmode_tintstrength(): number;

    display_nightmode_uiexp(): number;

    fan_control_mode(): number;

    idle_backlight_dim_ac_seconds(): number;

    idle_backlight_dim_battery_seconds(): number;

    idle_suspend_ac_seconds(): number;

    idle_suspend_battery_seconds(): number;

    idle_suspend_supressed(): boolean;

    is_adaptive_brightness_available(): boolean;

    is_display_brightness_available(): boolean;

    is_display_colormanagement_available(): boolean;

    is_display_colortemp_available(): boolean;

    is_fan_control_available(): boolean;

    is_wifi_powersave_enabled(): boolean;
}
