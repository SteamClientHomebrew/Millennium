import { Unregisterable } from "./shared";

/**
 * Represents the console functionality for executing commands and handling spew output.
 */
export interface Console {
    /**
     * Executes a console command.
     * @param command The command to execute in the console.
     */
    ExecCommand(command: string): void;

    /**
     * Retrieves autocomplete suggestions for a given console command.
     * @param command The console command to provide autocomplete suggestions for.
     * @returns an array of autocomplete suggestions.
     */
    GetAutocompleteSuggestions(command: string): Promise<string[]>;

    /**
     * Registers a callback function to receive spew output.
     * @param callback The callback function that will receive spew output.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForSpewOutput(callback: (output: SpewOutput) => void): Unregisterable;
}

export type SpewType_t = "assert" | "error" | "warning" | "info" | "input";

/**
 * Represents spew output information.
 */
export interface SpewOutput {
    /**
     * The content of the spew output.
     */
    spew: string;

    /**
     * The type or category of the spew output.
     */
    spew_type: SpewType_t;
}
