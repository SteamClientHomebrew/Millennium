import { BrowserContext } from "./shared";

export interface ClientNotifications {
    /**
     * Displays a Steam notification.
     * @param notification Notification type.
     * @param options Stringified object of {@link SteamNotificationOptions}.
     * @param callback
     */
    DisplayClientNotification(
        notification: EClientUINotificationType,
        options: string,
        callback: (context: BrowserContext) => void,
    ): void;

    /**
     * @param notificationId The ID of the notification to handle.
     * @param handleAction `true` to execute the callback function associated with the notification.
     */
    OnRespondToClientNotification(notificationId: number, handleAction: boolean): void;
}

export interface SteamNotificationOptions {
    body: string;
    chatroomgroupid?: number;
    chatroomid?: number;
    icon?: string;
    state: string;
    /** A Steam64 ID. */
    steamid: string;
    tag?: string;
    title?: string;
}

export enum EClientUINotificationType {
    GroupChatMessage = 1,
    FriendChatMessage,
    FriendPersonaState,
}
