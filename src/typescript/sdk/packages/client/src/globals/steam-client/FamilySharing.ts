import { EResult, Unregisterable } from "./shared";

/**
 * Represents functions related to Steam Family Sharing.
 */
export interface FamilySharing {
    GetAvailableLenders(appId: number): Promise<Lender[]>;

    RegisterForKickedBorrower: Unregisterable;

    SetPreferredLender(appId: number, param1: number): Promise<EResult>;
}

interface LenderDLC {
    rtStoreAssetModifyTime: number;
    strHeaderFilename: string;
    strName: string;
    unAppID: number;
}

export interface Lender {
    /**
     * A Steam64 ID.
     */
    steamid: string;
    appid: number;
    numDlc: number;
    bPreferred: boolean;
    vecDLC: LenderDLC[];
}
