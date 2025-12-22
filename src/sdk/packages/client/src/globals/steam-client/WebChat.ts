import {OverlayBrowserInfo} from "./Overlay";
import { EResult, EUIMode, Unregisterable } from "./shared";

export interface WebChat {
    BSuppressPopupsInRestore(): Promise<boolean>;

    /**
     * Gets your Steam3 ID.
     * @returns a Steam3 ID.
     */
    GetCurrentUserAccountID(): Promise<number>;

    /**
     * Gets the current user's 64x64 avatar as a data URL.
     * @returns the data URL.
     */
    GetLocalAvatarBase64(): Promise<string>;

    /**
     * Gets the current user's nickname.
     * @returns the nickname.
     */
    GetLocalPersonaName(): Promise<string>;

    GetOverlayChatBrowserInfo(): Promise<OverlayBrowserInfo[]>;

    // param0 - appid ?
    GetPrivateConnectString(param0: number): Promise<string>;

    /**
     * Gets information about push-to-Talk.
     * @returns
     */
    GetPushToTalkEnabled(): Promise<PushToTalkInfo>;

    /**
     * Gets the "Sign in to friends when Steam starts" option value.
     * @returns a boolean indicating whether the option is enabled or not.
     */
    GetSignIntoFriendsOnStart(): Promise<boolean>;

    /**
     * Retrieves the current UI mode.
     * @returns the current UI mode.
     */
    GetUIMode(): Promise<EUIMode>;

    OnGroupChatUserStateChange(chatGroupId: number, accountId: number, action: number): void;

    OnNewGroupChatMsgAdded(
        groupId: string,
        chatId: string,
        accountId: number,
        timestamp: number,
        param4: number,
        message: string,
    ): void;

    /**
     * Opens a provided URL in the Steam client. Does NOT work on desktop mode -
     * will open in default web browser instead!
     * @param url The URL to open.
     */
    OpenURLInClient(url: string, pid: number, forceExternal: boolean): void;

    /**
     * Registers a callback function to be called when the computer's active state changes.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     * @todo Changes to 2 after 10 seconds?
     * @todo Does not affect the keyboard?
     */
    RegisterForComputerActiveStateChange(
        callback: (state: EComputerActiveState, time: number) => void,
    ): Unregisterable;

    /**
     * @todo WebChat.ShowFriendChatDialog does this.
     */
    RegisterForFriendPostMessage(callback: (data: FriendChatDialogData) => void): Unregisterable;

    /**
     * To unregister, use {@link UnregisterForMouseXButtonDown}.
     */
    RegisterForMouseXButtonDown(callback: (param0: number) => void): void;

    /**
     * Registers a callback function to be called when the push-to-talk state changes.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForPushToTalkStateChange(callback: (state: boolean) => void): Unregisterable;

    /**
     * Registers a callback function to be called when the UI mode is changed.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForUIModeChange(callback: (mode: EUIMode) => void): Unregisterable;

    RegisterOverlayChatBrowserInfoChanged(callback: () => void): Unregisterable;

    SetActiveClanChatIDs(clanChatIds: number[]): void;

    SetNumChatsWithUnreadPriorityMessages(size: number): void;

    SetPersonaName(value: string): Promise<SetPersonaNameResult>;

    SetPushToMuteEnabled(value: boolean): void;

    SetPushToTalkEnabled(value: boolean): void;

    SetPushToTalkHotKey(param0: number): void;

    SetPushToTalkMouseButton(param0: number): void;

    SetVoiceChatActive(value: boolean): void;
    SetVoiceChatStatus(muted: boolean, deafened: boolean): void;
    ShowChatRoomGroupDialog(param0: number, param1: number): void;

    ShowFriendChatDialog(steamid64: string): void;

    UnregisterForMouseXButtonDown(): void;
}

export enum EComputerActiveState {
    Invalid,
    Active,
    Idle,
}

export interface FriendChatDialog {
    browserid: number;
    btakefocus: string;
    command: string;
    pid: number;
    steamid: string;
}

export interface FriendChatDialogData {
    data: FriendChatDialog;
}

interface SetPersonaNameResult {
    eResult: EResult;
    /**
     * Localization token. Empty if success.
     */
    strMessageToken: string;
}

export interface PushToTalkInfo {
    /** Indicates whether push-to-talk is enabled. */
    bEnabled: boolean;
    /** Indicates whether push-to-mute is in use instead. */
    bPushToMute: boolean;
    /**
     * Push-to-talk hotkey.
     * @todo enum? this is not EHIDKeyboardKey
     */
    vkHotKey: number;
    /** Push-to-talk hotkey name. */
    strKeyName: string;
}
