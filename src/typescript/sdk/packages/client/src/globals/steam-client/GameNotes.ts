import { EResult, OperationResponse } from "./shared";

export interface GameNotes {
    /**
     * @returns a boolean indicating whether the operation was successful.
     */
    DeleteImage(param0: string): Promise<boolean>;

    /**
     * @returns a boolean indicating whether the operation was successful.
     */
    DeleteNotes(param0: string): Promise<boolean>;

    GetNotes(filenameForNotes: string, directoryForNoteImages: string): Promise<Notes>;

    GetNotesMetadata(note: string): Promise<NoteMetadata>;
    GetNumNotes(): Promise<number>;
    GetQuota: Promise<NotesQuota>;

    IterateNotes(appId: number, length: number): Promise<NoteMetadata[]>;
    ResolveSyncConflicts(param0: boolean): Promise<EResult>;

    /**
     * @param notes Escaped JSON array of {@link Note}.
     */
    SaveNotes(filenameForNotes: string, notes: string): Promise<EResult>;
    SyncToClient(): Promise<EResult>;

    SyncToServer(): Promise<EResult>;

    /**
     * @param mimeType Image MIME type.
     * @param base64 Image contents in base64.
     * @returns an image file name with its extension that's meant to be used as a part of some URL. (todo)
     * @throws OperationResponse if invalid MIME type or unable to parse base64 BUT NOT if it failed.
     */
    UploadImage(imageFileNamePrefix: string, mimeType: string, base64: string): Promise<EResult | OperationResponse>;
}

export interface Note {
    appid: number;
    id: string;
    /**
     * Note contents in BB code.
     */
    content: string;
    ordinal: number;
    time_created: number;
    time_modified: number;
    title: string;
}

interface Notes {
    result: EResult;
    /**
     * Escaped JSON array of {@link Note}. Not present if {@link result} is {@link EResult.FileNotFound}.
     */
    notes?: string;
}

interface NoteMetadata {
    filename: string;
    filesize: number;
    result: EResult;
    timestamp: number;
}

interface NotesQuota {
    bytes: number;
    bytesAvailable: number;
    numFiles: number;
    numFilesAvailable: number;
}
