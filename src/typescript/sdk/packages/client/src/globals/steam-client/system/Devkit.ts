import { Unregisterable } from "../shared";

export interface Devkit {
    DeveloperModeChanged(state: boolean): any;

    RegisterForPairingPrompt(callback: (param0: any) => any): Unregisterable;

    RespondToPairingPrompt(param0: any, param1: any): any;

    SetPairing(param0: any): any;
}
