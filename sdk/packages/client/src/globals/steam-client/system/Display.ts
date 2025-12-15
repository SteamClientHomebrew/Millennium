import { Unregisterable } from "../shared";

export interface Display {
    EnableUnderscan(param0: boolean): any;

    RegisterForBrightnessChanges(callback: (state: BrightnessState) => void): Unregisterable;

    SetBrightness(brightness: number): any;

    SetUnderscanLevel(param0: any): any;
}

export interface BrightnessState {
    flBrightness: number;
}
