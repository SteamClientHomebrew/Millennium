import { VDFBoolean_t } from "./shared";

/**
 * Represents friend settings and configuration.
 */
export interface FriendSettings {
    /**
     * Retrieves a list of enabled friend settings features.
     * @returns an array of enabled friend settings features.
     */
    GetEnabledFeatures(): Promise<FriendSettingsFeatureObject[]>;

    /**
     * Registers a callback function to be notified of friend settings changes.
     * @param callback The callback function to be called when friend settings change.
     * @remarks The callback receives a JSON object string which may be parsed into {@link FriendSettingsChange}.
     */
    RegisterForSettingsChanges(callback: (settings: string) => void): void;

    /**
     * @param details Stringified {@link FriendSettingsChange}.
     */
    SetFriendSettings(details: string): void;
}

export enum EChatFlashMode {
    Always,
    Minimized,
    Never,
}

export interface FriendSettingsFeatureObject {
    feature: FriendSettingsFeature_t;
    bEnabled: boolean;
}

export type FriendSettingsFeature_t =
    | "DoNotDisturb"
    | "FriendsFilter"
    | "LoaderWindowSynchronization"
    | "NewVoiceHotKeyState"
    | "NonFriendMessageHandling"
    | "PersonaNotifications"
    | "ServerVirtualizedMemberLists"
    | "SteamworksChatAPI";

export type FriendSettingsEnabledFeatures<T> = {
    [feature in FriendSettingsFeature_t]: T;
};

export interface FriendSettingsChange {
    bNotifications_ShowIngame: VDFBoolean_t;
    bNotifications_ShowOnline: VDFBoolean_t;
    bNotifications_ShowMessage: VDFBoolean_t;
    bNotifications_EventsAndAnnouncements: VDFBoolean_t;
    bSounds_PlayIngame: VDFBoolean_t;
    bSounds_PlayOnline: VDFBoolean_t;
    bSounds_PlayMessage: VDFBoolean_t;
    bSounds_EventsAndAnnouncements: VDFBoolean_t;
    bAlwaysNewChatWindow: VDFBoolean_t;
    bForceAlphabeticFriendSorting: VDFBoolean_t;
    nChatFlashMode: EChatFlashMode;
    bRememberOpenChats: VDFBoolean_t;
    bCompactQuickAccess: VDFBoolean_t;
    bCompactFriendsList: VDFBoolean_t;
    bNotifications_ShowChatRoomNotification: VDFBoolean_t;
    bSounds_PlayChatRoomNotification: VDFBoolean_t;
    bHideOfflineFriendsInTagGroups: VDFBoolean_t;
    bHideCategorizedFriends: VDFBoolean_t;
    bCategorizeInGameFriendsByGame: VDFBoolean_t;
    nChatFontSize: number;
    b24HourClock: VDFBoolean_t;
    bDoNotDisturbMode: VDFBoolean_t;
    bDisableEmbedInlining: VDFBoolean_t;
    bSignIntoFriends: VDFBoolean_t;
    bDisableSpellcheck: VDFBoolean_t;
    bDisableRoomEffects: VDFBoolean_t;
    bAnimatedAvatars: VDFBoolean_t;
    featuresEnabled: FriendSettingsEnabledFeatures<VDFBoolean_t>;
}
