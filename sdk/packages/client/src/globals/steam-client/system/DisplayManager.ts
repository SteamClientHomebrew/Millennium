import { Unregisterable } from "../shared";

// CMsgSystemDisplayManagerState, CMsgSystemDisplayManagerSetMode
export interface DisplayManager {
    ClearModeOverride(displayId: any): any;

    GetState: any;

    RegisterForStateChanges(callback: () => void): Unregisterable;

    SetCompatibilityMode(displayId: any): any;

    SetGamescopeInternalResolution(width: number, height: number): any;

    SetMode(base64: string): any; //
}
