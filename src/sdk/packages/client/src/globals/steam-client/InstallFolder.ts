import { Unregisterable } from "./shared";
import {EAppUpdateError} from "./App";

/**
 * Represents functions related to Steam Install Folders.
 */
export interface InstallFolder {
    /**
     * Adds a Steam Library folder to the Steam client.
     * @param path The path of the Steam Library folder to be added.
     * @returns the index of the added folder.
     */
    AddInstallFolder(path: string): Promise<number>;

    /**
     * Opens the file explorer to browse files in a specific Steam Library folder.
     * @param folderIndex The index of the folder to be opened.
     */
    BrowseFilesInFolder(folderIndex: number): void;

    /**
     * Cancels the current move operation for moving game content.
     */
    CancelMove(): void;

    /**
     * Retrieves a list of installed Steam Library folders.
     * @returns an array of SteamInstallFolder objects.
     */
    GetInstallFolders(): Promise<SteamInstallFolder[]>;

    /**
     * Retrieves a list of potential Steam Library folders that can be added.
     * @returns an array of PotentialInstallFolder objects.
     */
    GetPotentialFolders(): Promise<PotentialInstallFolder[]>;

    /**
     * Moves the installation folder for a specific app to another Steam Library folder.
     * @param appId The ID of the application to be moved.
     * @param folderIndex The index of the target Steam Library folder.
     */
    MoveInstallFolderForApp(appId: number, folderIndex: number): void;

    /**
     * Refreshes the list of installed Steam Library folders.
     */
    RefreshFolders(): void;

    /**
     * Registers a callback function to be called when changes occur in Steam Install Folders.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForInstallFolderChanges(callback: (change: FolderChange) => void): Unregisterable;

    /**
     * Registers a callback function to be called when moving game content progresses.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForMoveContentProgress(callback: (progress: MoveContentProgress) => void): Unregisterable;

    /**
     * Registers a callback function to be called when repairing an install folder is finished.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForRepairFolderFinished(callback: (change: FolderChange) => void): Unregisterable;

    /**
     * Removes a Steam Library folder from the Steam client.
     * @param folderIndex The index of the folder to be removed.
     */
    RemoveInstallFolder(folderIndex: number): void;

    /**
     * Repairs an installed Steam Library folder.
     * @param folderIndex The index of the folder to be repaired.
     */
    RepairInstallFolder(folderIndex: number): void;

    /**
     * Sets a specific Steam Library folder as the default install folder.
     * @param folderIndex The index of the folder to be set as default.
     */
    SetDefaultInstallFolder(folderIndex: number): void;

    /**
     * Sets a user-defined label for a specific Steam Library folder.
     * @param folderIndex The index of the folder to be labeled.
     * @param label The label to be assigned to the folder.
     */
    SetFolderLabel(folderIndex: number, label: string): void;
}

/**
 * Represents information about an installation folder.
 */
export interface SteamInstallFolder extends PotentialInstallFolder {
    /** Index of the folder. */
    nFolderIndex: number;
    /** Used space in the folder. */
    strUsedSize: string;
    /** Size of DLC storage used in the folder. */
    strDLCSize: string;
    /** Size of workshop storage used in the folder. */
    strWorkshopSize: string;
    /** Size of staged storage used in the folder. */
    strStagedSize: string;
    /** Indicates if the folder is set as the default installation folder. */
    bIsDefaultFolder: boolean;
    /** Indicates if the folder is currently mounted. */
    bIsMounted: boolean;
    /** List of applications installed in the folder. */
    vecApps: AppInfo[];
}

export interface PotentialInstallFolder {
    /** Path of the folder. */
    strFolderPath: string;
    /** User label for the folder. */
    strUserLabel: string;
    /** Name of the drive where the folder is located. */
    strDriveName: string;
    /** Total capacity of the folder. */
    strCapacity: string;
    /** Available free space in the folder. */
    strFreeSpace: string;
    /** Indicates if the folder is on a fixed drive. */
    bIsFixed: boolean;
}

/**
 * Represents information about an installed application.
 */
export interface AppInfo {
    /** ID of the application. */
    nAppID: number;
    /** Name of the application. */
    strAppName: string;
    /** Sorting information for the application. */
    strSortAs: string;
    /** Last played time in Unix Epoch time format. */
    rtLastPlayed: number;
    /** Size of used storage by the application. */
    strUsedSize: string;
    /** Size of DLC storage used by the application. */
    strDLCSize: string;
    /** Size of workshop storage used by the application. */
    strWorkshopSize: string;
    /** Size of staged storage used by the application. */
    strStagedSize: string;
}

export interface FolderChange {
    folderIndex: number;
}

export interface MoveContentProgress {
    appid: number;
    eError: EAppUpdateError;
    flProgress: number;
    strBytesMoved: string;
    strTotalBytesToMove: string;
    nFilesMoved: number;
}
