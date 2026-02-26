import { EResult, Unregisterable } from "./shared";
import {LaunchOption} from "./App";

export interface Streaming {
    AcceptStreamingEULA(appId: number, id: string, version: number): void;

    CancelStreamGame(): void; // existing stream

    /**
     * Registers a callback function to be called when the streaming client finishes.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStreamingClientFinished(callback: (code: EResult, result: string) => void): Unregisterable;

    /**
     * Registers a callback function to be called when there is progress in the launch of the streaming client.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStreamingClientLaunchProgress(
        callback: (actionType: string, taskDetails: string, done: number, total: number) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when the streaming client is started (e.g., when clicking the stream button).
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStreamingClientStarted(callback: (appId: number) => void): Unregisterable;

    /**
     * Registers a callback function to be called when the streaming launch is complete.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStreamingLaunchComplete(callback: (code: EResult, result: string) => void): Unregisterable;

    RegisterForStreamingShowEula(callback: (appId: number) => void): Unregisterable;

    RegisterForStreamingShowIntro(callback: (appId: number, param: string) => void): Unregisterable;

    /**
     * Registers a callback function to be called when the streaming client receives launch options from the host.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStreamingShowLaunchOptions(
        callback: (appId: number, launchOptions: LaunchOption[]) => void,
    ): Unregisterable; // Callback when streaming client receives launch options from host

    StreamingContinueStreamGame(): void; // existing game running on another streaming capable device

    /**
     * Chooses the launch option for the streamed app by its index
     * and restarts the stream.
     */
    StreamingSetLaunchOption(index: number): void;
}
