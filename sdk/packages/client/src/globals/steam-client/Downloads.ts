import { Unregisterable } from "./shared";
import {EAppUpdateError} from "./App";

/**
 * Represents functions related to managing downloads in Steam.
 */
export interface Downloads {
    /**
     * Enables or disables all downloads in Steam.
     * @param enable True to enable downloads, false to disable.
     */
    EnableAllDownloads(enable: boolean): void;

    /**
     * Moves the update for a specific app down the download queue.
     * @param appId The ID of the application to move.
     */
    MoveAppUpdateDown(appId: number): void;

    /**
     * Moves the update for a specific app up the download queue.
     * @param appId The ID of the application to move.
     */
    MoveAppUpdateUp(appId: number): void;

    PauseAppUpdate(appId: number): void; // Broken? It seems to be removing it from download list like RemoveFromDownloadList

    /**
     * Adds the update for a specific app to the download queue.
     * @param appId The ID of the application to queue.
     */
    QueueAppUpdate(appId: number): void;

    /**
     * Registers a callback function to be called when download items change.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForDownloadItems(
        callback: (isDownloading: boolean, downloadItems: DownloadItem[]) => void,
    ): Unregisterable;

    /**
     * Registers a callback function to be called when download overview changes.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForDownloadOverview(callback: (overview: DownloadOverview) => void): Unregisterable;

    /**
     * Removes the update for a specific app from the download list and places it in the unscheduled list.
     * @param appId The ID of the application to remove.
     */
    RemoveFromDownloadList(appId: number): void;

    /**
     * Resumes the update for a specific app in the queue.
     * @param appId The ID of the application to resume.
     */
    ResumeAppUpdate(appId: number): void;

    /**
     * Sets an app to launch when its download is complete.
     * @param appId The ID of the application to set.
     */
    SetLaunchOnUpdateComplete(appId: number): void;

    /**
     * Sets the queue index for an app in the download queue.
     * @param appId The ID of the application to set the index for.
     * @param index The index to set.
     * @remarks Index of 0 is the current download in progress.
     */
    SetQueueIndex(appId: number, index: number): void;

    /**
     * Suspends or resumes download throttling.
     * @param suspend Whether to suspend or resume download throttling.
     */
    SuspendDownloadThrottling(suspend: boolean): void;

    /**
     * Suspends or resumes local transfers.
     * @param suspend Whether to suspend or resume local transfers.
     */
    SuspendLanPeerContent(suspend: boolean): void;
}

export interface DownloadItem {
    /** True if this app is currently downloading */
    active: boolean;
    /** Appid of app */
    appid: number;
    /** Current build ID for the installed app, zero if the app isn't installed yet */
    buildid: number;
    /** True if this update has been completed */
    completed: boolean;
    /** For completed downloads, time of completion, 0 if not completed */
    completed_time: number;
    deferred_time: number;
    /** Bytes already downloaded, sum across all content types */
    downloaded_bytes: number;
    /** If true, game will launch when its download completes successfully */
    launch_on_completion: boolean;
    /** True if this app has been paused by the user or the system */
    paused: boolean;
    /** Queue index, -1 if the item is unqueued */
    queue_index: number;
    /** Build ID that this download is moving towards. This can be the same as buildid.*/
    target_buildid: number;
    /** Total bytes to download, sum across all content types */
    total_bytes: number;
    /**
     * Update error description, when paused and there has been an error.
     * Unlocalized and shouldn't be displayed to the user.
     */
    update_error: string;
    update_result: EAppUpdateError;
    update_type_info: UpdateTypeInfo[];
}

export interface DownloadOverview {
    /** Set if we are downloading from LAN peer content server */
    lan_peer_hostname: string;
    /** True if all downloads are paused */
    paused: boolean;
    /** True if download throttling has been temporarily suspended for the current download */
    throttling_suspended: boolean;
    /** Appid of currently updating app */
    update_appid: number;
    /** Bytes already downloaded */
    update_bytes_downloaded: number;
    /** Bytes already processed in current phase - resets to zero when update stage changes */
    update_bytes_processed: number;
    /** Bytes already staged */
    update_bytes_staged: number;
    /** Total bytes to download */
    update_bytes_to_download: number;
    /** Total bytes to process in current phase - resets to zero when update stage changes */
    update_bytes_to_process: number;
    /** Total bytes to be staged */
    update_bytes_to_stage: number;
    /** Current disk throughput estimate */
    update_disc_bytes_per_second: number;
    /** True if the current update is an initial install */
    update_is_install: boolean;
    /** True if download and staging sizes are prefetch estimates */
    update_is_prefetch_estimate: boolean;
    /** True if the current update is for shader update */
    update_is_shader: boolean;
    /** True if the client is running in peer content server mode serving other peers */
    update_is_upload: boolean;
    /** True if the current update is for workshop content */
    update_is_workshop: boolean;
    /** Current bandwidth estimate for download */
    update_network_bytes_per_second: number;
    /** Peak bandwidth estimate for download */
    update_peak_network_bytes_per_second: number;
    /** Estimate of remaining time (in seconds) until download completes (not including staging) */
    update_seconds_remaining: number;
    /** Time current update started */
    update_start_time: number;
    update_state: 'None' | 'Starting' | 'Updating' | 'Stopping';
}

export interface UpdateTypeInfo {
    /** True if this content type had an update and it has completed */
    completed_update: boolean;
    /** Bytes already downloaded for this content type */
    downloaded_bytes: number;
    /** True if this content type has or had an update */
    has_update: boolean;
    /** Total bytes to download for this content type */
    total_bytes: number;
}
