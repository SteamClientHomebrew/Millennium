export interface Cloud {
    /**
     * Resolves a synchronization conflict for an app in the cloud.
     * @param appId The ID of the app with the sync conflict.
     * @param keepLocal Whether to keep the local version during conflict resolution.
     */
    ResolveAppSyncConflict(appId: number, keepLocal: boolean): void;

    /**
     * Retries syncing an app with the cloud.
     * @param appId The ID of the app to retry syncing.
     */
    RetryAppSync(appId: number): void;
}
