import { Unregisterable } from "./shared";


export interface OpenVR {
    Device: VRDevice;
    DeviceProperties: DeviceProperties;

    /**
     * @throws OperationResponse if mutual capabilities haven't been loaded.
     */
    GetMutualCapabilities(): Promise<any>;

    GetWebSecret(): Promise<string>;

    InstallVR(): any;

    Keyboard: Keyboard;
    PathProperties: PathProperties;

    QuitAllVR(): any;

    RegisterForButtonPress: Unregisterable;

    RegisterForHMDActivityLevelChanged(callback: (m_eHMDActivityLevel: EHMDActivityLevel) => void): Unregisterable;

    RegisterForInstallDialog: Unregisterable;

    RegisterForStartupErrors(callback: (clientError: any, initError: any, initErrorString: string) => void): Unregisterable;

    RegisterForVRHardwareDetected(callback: (m_bHMDPresent: any, m_bHMDHardwareDetected: any, m_strHMDName: any) => void): Unregisterable;

    RegisterForVRModeChange(callback: (m_bIsVRRunning: boolean) => void): Unregisterable;

    RegisterForVRSceneAppChange(callback: (param0: number) => void): Unregisterable;

    SetOverlayInteractionAffordance: any;

    StartVR: any;
    TriggerOverlayHapticEffect: any;
    VRNotifications: VRNotifications;
    VROverlay: VROverlay;
}

export interface VRDevice {
    BIsConnected: any;
    RegisterForDeviceConnectivityChange: Unregisterable;

    RegisterForVRDeviceSeenRecently(callback: (m_bVRDeviceSeenRecently: any) => void): Unregisterable;
}

export interface DeviceProperties {
    GetBoolDeviceProperty: any;
    GetDoubleDeviceProperty: any;
    GetFloatDeviceProperty: any;
    GetInt32DeviceProperty: any;
    GetStringDeviceProperty: any;
    RegisterForDevicePropertyChange: Unregisterable;
}

export interface Keyboard {
    Hide(): any;

    /**
     * {@link EKeyboardFlags} could be useful here
     */
    RegisterForStatus(callback: (m_bIsKeyboardOpen: boolean, m_eKeyboardFlags: number, m_sInitialKeyboardText: string) => void): Unregisterable;

    SendDone(): any;

    SendText(key: string): any; //???
    Show(): any;
}

export interface PathProperties {
    GetBoolPathProperty: any;
    GetDoublePathProperty: any;
    GetFloatPathProperty: any;
    GetInt32PathProperty: any;
    GetStringPathProperty: any;
    RegisterForPathPropertyChange: any;
    SetBoolPathProperty: any;
    SetDoublePathProperty: any;
    SetFloatPathProperty: any;
    SetInt32PathProperty: any;
    SetStringPathProperty: any;
}

export interface VRNotifications {
    HideCustomNotification: any;
    RegisterForNotificationEvent: Unregisterable;
    ShowCustomNotification: any;
}

export interface VROverlay {
    HideDashboard: any;

    IsDashboardVisible(): Promise<boolean>;

    RegisterForButtonPress: Unregisterable;
    RegisterForCursorMovement: Unregisterable;
    RegisterForThumbnailChanged: Unregisterable;
    RegisterForVisibilityChanged: Unregisterable;
    ShowDashboard: any;

    SwitchToDashboardOverlay(param0: string): void;
}

export enum EHMDActivityLevel {
    Unknown = -1,
    Idle,
    UserInteraction,
    UserInteraction_Timeout,
    Standby,
    Idle_Timeout,
}

export enum EKeyboardFlags {
    Minimal = 1 << 0,
    Modal = 1 << 1,
    ShowArrowKeys = 1 << 2,
    HideDoneKey = 1 << 3,
}
