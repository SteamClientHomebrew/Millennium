import { JsPbMessage, Unregisterable } from "./shared";

/**
 * Everything is taken from here:
 * https://github.com/SteamDatabase/SteamTracking/blob/master/Protobufs/steammessages_clientnotificationtypes.proto
 */
export interface Notifications {
    /**
     * If `data` is deserialized, returns one of the following here: {@link Notifications}
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForNotifications(
        callback: (notificationIndex: number, type: EClientNotificationType, data: ArrayBuffer) => void,
    ): Unregisterable;
}

export enum EClientNotificationType {
    Invalid,
    DownloadComplete,
    FriendInvite,
    FriendInGame,
    FriendOnline,
    Achievement,
    LowBattery,
    SystemUpdate,
    FriendMessage,
    GroupChatMessage,
    FriendInviteRollup,
    FamilySharingDeviceAuthorizationChanged,
    FamilySharingStopPlaying,
    FamilySharingLibraryAvailable,
    Screenshot,
    CloudSyncFailure,
    CloudSyncConflict,
    IncomingVoiceChat,
    ClaimSteamDeckRewards,
    GiftReceived,
    ItemAnnouncement,
    HardwareSurvey,
    LowDiskSpace,
    BatteryTemperature,
    DockUnsupportedFirmware,
    PeerContentUpload,
    CannotReadControllerGuideButton,
    Comment,
    Wishlist,
    TradeOffer,
    AsyncGame,
    General,
    HelpRequest,
    OverlaySplashScreen,
    BroadcastAvailableToWatch,
    TimedTrialRemaining,
    LoginRefresh,
    MajorSale,
    TimerExpired,
    ModeratorMsg,
    SteamInputActionSetChanged,
    RemoteClientConnection,
    RemoteClientStartStream,
    StreamingClientConnection,
    FamilyInvite,
    PlaytimeWarning,
    FamilyPurchaseRequest,
    FamilyPurchaseRequestResponse,
    ParentalFeatureRequest,
    ParentalPlaytimeRequest,
    GameRecordingError,
    ParentalFeatureResponse,
    ParentalPlaytimeResponse,
    RequestedGameAdded,
    ClipDownloaded,
    GameRecordingStart,
    GameRecordingStop,
    GameRecordingUserMarkerAdded,
    GameRecordingInstantClip,
}

export enum ESystemUpdateNotificationType {
    Invalid,
    Available,
    NeedsRestart,
}

export enum EGameRecordingErrorType {
    General = 1,
    LowDiskSpace,
}

export interface ClientNotificationGroupChatMessage extends JsPbMessage {
    tag(): string;

    /** A Steam64 ID. */
    steamid_sender(): string;

    chat_group_id(): string;

    chat_id(): string;

    title(): string;

    body(): string;

    rawbody(): string;

    icon(): string;

    notificationid(): number;
}

export interface ClientNotificationFriendMessage extends JsPbMessage {
    body(): string;

    icon(): string;

    notificationid(): number;

    response_steamurl(): string;

    /** A Steam64 ID. */
    steamid(): string;

    tag(): string;

    title(): string;
}

export interface ClientNotificationCloudSyncFailure extends JsPbMessage {
	appid(): number;
}

export interface ClientNotificationCloudSyncConflict extends JsPbMessage {
	appid(): number;
}

export interface ClientNotificationScreenshot extends JsPbMessage {
	screenshot_handle(): string;
	description(): string;
	local_url(): string;
}

export interface ClientNotificationDownloadCompleted extends JsPbMessage {
	appid(): number;
	dlc_appid(): number;
}

export interface ClientNotificationFriendInvite extends JsPbMessage {
	steamid(): number;
}

export interface ClientNotificationFriendInviteRollup extends JsPbMessage {
	new_invite_count(): number;
}

export interface ClientNotificationFriendInGame extends JsPbMessage {
	steamid(): number;
	game_name(): string;
}

export interface ClientNotificationFriendOnline extends JsPbMessage {
	steamid(): number;
}

export interface ClientNotificationAchievement extends JsPbMessage {
	achievement_id(): string;
	appid(): number;
	name(): string;
	description(): string;
	image_url(): string;
	achieved(): boolean;
	rtime_unlocked(): number;
	min_progress(): number;
	current_progress(): number;
	max_progress(): number;
	global_achieved_pct(): number;
}

export interface ClientNotificationLowBattery extends JsPbMessage {
	pct_remaining(): number;
}

export interface ClientNotificationSystemUpdate extends JsPbMessage {
	type(): ESystemUpdateNotificationType;
}

export interface ClientNotificationFriendMessage extends JsPbMessage {
	tag(): string;
	steamid(): string;
	title(): string;
	body(): string;
	icon(): string;
	notificationid(): number;
	response_steamurl(): string;
}

export interface ClientNotificationGroupChatMessage extends JsPbMessage {
	tag(): string;
	steamid_sender(): string;
	chat_group_id(): string;
	chat_id(): string;
	title(): string;
	body(): string;
	rawbody(): string;
	icon(): string;
	notificationid(): number;
}

export interface ClientNotificationFamilySharingDeviceAuthorizationChanged extends JsPbMessage {
	accountid_owner(): number;
	authorized(): boolean;
}

export interface ClientNotificationFamilySharingStopPlaying extends JsPbMessage {
	accountid_owner(): number;
	seconds_remaining(): number;
	appid(): number;
}

export interface ClientNotificationFamilySharingLibraryAvailable extends JsPbMessage {
	accountid_owner(): number;
}

export interface ClientNotificationIncomingVoiceChat extends JsPbMessage {
	steamid(): number;
}

export interface ClientNotificationClaimSteamDeckRewards extends JsPbMessage {
}

export interface ClientNotificationGiftReceived extends JsPbMessage {
	sender_name(): string;
}

export interface ClientNotificationItemAnnouncement extends JsPbMessage {
	new_item_count(): number;
	new_backpack_items(): boolean;
}

export interface ClientNotificationHardwareSurveyPending extends JsPbMessage {
}

export interface ClientNotificationLowDiskSpace extends JsPbMessage {
	folder_index(): number;
}

export interface ClientNotificationBatteryTemperature extends JsPbMessage {
	temperature(): number;
	notification_type(): string;
}

export interface ClientNotificationDockUnsupportedFirmware extends JsPbMessage {
}

export interface ClientNotificationPeerContentUpload extends JsPbMessage {
	appid(): number;
	peer_name(): string;
}

export interface ClientNotificationCannotReadControllerGuideButton extends JsPbMessage {
	controller_index(): number;
}

export interface ClientNotificationOverlaySplashScreen extends JsPbMessage {
}

export interface ClientNotificationBroadcastAvailableToWatch extends JsPbMessage {
	broadcast_permission(): number;
}

export interface ClientNotificationTimedTrialRemaining extends JsPbMessage {
	appid(): number;
	icon(): string;
	offline(): boolean;
	allowed_seconds(): number;
	played_seconds(): number;
}

export interface ClientNotificationLoginRefresh extends JsPbMessage {
}

export interface ClientNotificationTimerExpired extends JsPbMessage {
}

export interface ClientNotificationSteamInputActionSetChanged extends JsPbMessage {
	controller_index(): number;
	action_set_name(): string;
}

export interface ClientNotificationRemoteClientConnection extends JsPbMessage {
	machine(): string;
	connected(): boolean;
}

export interface ClientNotificationRemoteClientStartStream extends JsPbMessage {
	machine(): string;
	game_name(): string;
}

export interface ClientNotificationStreamingClientConnection extends JsPbMessage {
	hostname(): string;
	machine(): string;
	connected(): boolean;
}

export interface ClientNotificationPlaytimeWarning extends JsPbMessage {
	type(): string;
	playtime_remaining(): number;
}

export interface ClientNotificationGameRecordingError extends JsPbMessage {
	game_id(): number;
	error_type(): EGameRecordingErrorType;
}

export interface ClientNotificationGameRecordingStart extends JsPbMessage {
	game_id(): number;
}

export interface ClientNotificationGameRecordingStop extends JsPbMessage {
	game_id(): number;
	clip_id(): string;
	duration_secs(): number;
}

export interface ClientNotificationGameRecordingUserMarkerAdded extends JsPbMessage {
    game_id(): number;
}

export interface CClientNotificationGameRecordingInstantClip extends JsPbMessage {
    game_id(): number;
    clip_id(): string;
	duration_secs(): number;
}
