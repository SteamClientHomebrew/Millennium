/**
 * Interface for managing screenshots.
 */
export interface Screenshots {
    /**
     * Deletes a local screenshot.
     * @param appId The ID of the application.
     * @param screenshotIndex The index of the local screenshot.
     * @returns a boolean value indicating whether the deletion was successful.
     */
    DeleteLocalScreenshot(appId: string, screenshotIndex: number): Promise<boolean>;

    DeleteLocalScreenshots(screenshots: ScreenshotToDelete[]): Promise<ScreenshotDeletionResponse>;

    /**
     * Retrieves all local screenshots for all applications.
     * @returns an array of Screenshot objects.
     */
    GetAllAppsLocalScreenshots(): Promise<Screenshot[]>;

    /**
     * Retrieves the count of all local screenshots for all applications.
     * @returns the count of local screenshots.
     */
    GetAllAppsLocalScreenshotsCount(): Promise<number>;

    /**
     * Retrieves a range of local screenshots for all applications.
     * @param start The starting index of the screenshot range.
     * @param end The ending index of the screenshot range.
     * @returns an array of Screenshot objects within the specified range.
     */
    GetAllAppsLocalScreenshotsRange(start: number, end: number): Promise<Screenshot[]>;

    /**
     * Retrieves all local screenshots.
     * @returns an array of Screenshot objects.
     */
    GetAllLocalScreenshots(): Promise<Screenshot[]>;

    /**
     * Retrieves the game associated with a specific local screenshot index.
     * @param screenshotIndex The index of the local screenshot.
     * @returns the ID of the game associated with the screenshot.
     */
    GetGameWithLocalScreenshots(screenshotIndex: number): Promise<number>;

    /**
     * Retrieves the last taken local screenshot.
     * @returns the last taken local screenshot.
     */
    GetLastScreenshotTaken(): Promise<Screenshot>;

    /**
     * Retrieves a specific local screenshot for an application.
     * @param appId The ID of the application.
     * @param screenshotIndex The index of the local screenshot.
     * @returns the requested local screenshot.
     */
    GetLocalScreenshotByHandle(appId: string, screenshotIndex: number): Promise<Screenshot>;

    /**
     * Retrieves the count of local screenshots for a specific application.
     * @param appId The ID of the application.
     * @returns the count of local screenshots for the application.
     */
    GetLocalScreenshotCount(appId: number): Promise<number>;

    /**
     * Retrieves the path of a screenshot.
     * @param appId The ID of the application.
     * @param hHandle The handle of the screenshot.
     * @returns the screenshot path or the screenshot directory if no such handle.
     */
    GetLocalScreenshotPath(appId: number, hHandle: number): Promise<string>;

    /**
     * Retrieves the number of games with local screenshots.
     * @returns the number of games with local screenshots.
     */
    GetNumGamesWithLocalScreenshots(): Promise<number>;

    /**
     * Gets total screenshot usage in the specified library folder.
     * @param path Library folder path.
     * @returns the number of taken space in bytes.
     */
    GetTotalDiskSpaceUsage(path: string): Promise<number>;

    /**
     * Opens a local screenshot in the system image viewer.
     * If the screenshot index is invalid, this function opens the screenshots directory for the specified application ID.
     * @param appId The ID of the application.
     * @param screenshotIndex The index of the local screenshot.
     */
    ShowScreenshotInSystemViewer(appId: string, screenshotIndex: number): void;

    /**
     * Opens the folder containing local screenshots for a specific application.
     * @param appId The ID of the application.
     */
    ShowScreenshotsOnDisk(appId: string): void;

    /**
     * Uploads a local screenshot.
     * @param appId The ID of the application.
     * @param localScreenshot_hHandle The handle of the local screenshot.
     * @param filePrivacyState The privacy state of the screenshot file.
     * @returns a boolean value indicating whether the upload was successful.
     */
    UploadLocalScreenshot(
        appId: string,
        localScreenshot_hHandle: number,
        filePrivacyState: EUCMFilePrivacyState,
    ): Promise<boolean>;
}

export interface Screenshot {
    nAppID: number;
    strGameID: string;
    hHandle: number;
    nWidth: number;
    nHeight: number;
    nCreated: number; // timestamp
    ePrivacy: EUCMFilePrivacyState;
    strCaption: string;
    bSpoilers: boolean;
    strUrl: string;
    bUploaded: boolean;
    ugcHandle: string;
}

export interface ScreenshotToDelete {
    gameID: string;
    rgHandles: number[];
}

export interface ScreenshotDeletionResponse {
    bSuccess: boolean;
    rgFailedRequestIndices: number[];
}

export enum EUCMFilePrivacyState {
    Invalid = -1,
    Private = 1 << 1,
    FriendsOnly = 1 << 2,
    Public = 1 << 3,
    Unlisted = 1 << 4,
}
