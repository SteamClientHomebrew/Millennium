import { Unregisterable } from "./shared";
import {AppAchievements} from "./App";
import { Screenshot } from "./Screenshots";

/**
 * Represents functions related to Steam Game Sessions.
 */
export interface GameSessions {
    /**
     * Registers a callback function to be called when an achievement notification is received.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForAchievementNotification(
        callback: (notification: AchievementNotification) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when an app lifetime notification is received.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForAppLifetimeNotifications(
        callback: (notification: AppLifetimeNotification) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when a screenshot notification is received.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForScreenshotNotification(
        callback: (notification: ScreenshotNotification) => void,
    ): Unregisterable;
}

/**
 * @prop unAppID is not properly set by Steam for non-steam game shortcuts, so it defaults to 0 for them
 */
interface GameSessionNotificationBase {
  unAppID: number;
}

export interface AchievementNotification extends GameSessionNotificationBase {
    achievement: AppAchievements;
    nCurrentProgress: number;
    nMaxProgress: number;
}

export interface AppLifetimeNotification extends GameSessionNotificationBase {
    nInstanceID: number;
    bRunning: boolean;
}

export interface ScreenshotNotification extends GameSessionNotificationBase {
    details: Screenshot;
    hScreenshot: number;
    strOperation: "deleted" | "written";
}
