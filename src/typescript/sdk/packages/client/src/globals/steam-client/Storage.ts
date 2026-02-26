import { OperationResponse } from "./shared";

/**
 * SteamClient.MachineStorage affects the "STEAMPATH/config/config.vdf" file.
 * SteamClient.RoamingStorage affects the "STEAMPATH/userdata/STEAMID3/7/remote/sharedconfig.vdf" file.
 * SteamClient.Storage affects the "STEAMPATH/userdata/STEAMID3/config/localconfig.vdf" file.
 */
export interface Storage {
    DeleteKey(key: string): Promise<OperationResponse | undefined>;

    /**
     * @remarks Use {@link SetObject} to set.
     */
    GetJSON(key: string): Promise<OperationResponse | string>;

    GetString(key: string): Promise<OperationResponse | string>;

    /**
     * @remarks Use {@link SetObject} to get.
     */
    SetObject(key: string, value: object): Promise<OperationResponse | undefined>;

    SetString(key: string, value: string): Promise<OperationResponse | undefined>;
}
