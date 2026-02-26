import { Unregisterable } from "./shared";

/**
 * Represents functions related to managing friends in Steam.
 */
export interface Friends {
    /**
     * Adds a user to the friend list.
     * @param steamId The Steam ID of the user to add as a friend.
     * @returns `true` if the friend was added successfully.
     */
    AddFriend(steamId: string): Promise<boolean>;

    /**
     * @returns a list of players you recently played with.
     */
    GetCoplayData(): Promise<CoplayData>;

    InviteUserToCurrentGame(steam64Id: string, steamIdTarget: string): Promise<boolean>;

    /**
     * Invites a user to a specific game.
     * @param steamId The Steam ID of the user to invite.
     * @param appId The ID of the game to invite the user to.
     * @param connectString Additional parameters for the invitation.
     * @returns `true` if the user was invited successfully.
     */
    InviteUserToGame(steamId: string, appId: number, connectString: string): Promise<boolean>;

    /**
     * Invites a user to a specific lobby.
     * @returns `true` if the user was invited successfully.
     */
    InviteUserToLobby(steam64Id: string, steamIdTarget: string): Promise<boolean>;

    InviteUserToRemotePlayTogetherCurrentGame(steam64Id: string): Promise<boolean>;

    RegisterForMultiplayerSessionShareURLChanged(
        appId: number,
        callback: (param0: string, param1: string) => void,
    ): Unregisterable;

    RegisterForVoiceChatStatus(callback: (status: VoiceChatStatus) => void): Unregisterable;

    /**
     * Removes a user from the friend list.
     * @param steamId The Steam ID of the user to remove from the friend list.
     * @returns `true` if the friend was removed successfully.
     */
    RemoveFriend(steamId: string): Promise<boolean>;

    ShowRemotePlayTogetherUI(): void;
}

export interface CoplayData {
    currentUsers: CoplayUser[];
    recentUsers: CoplayUser[];
}

export interface CoplayUser {
    accountid: number;
    rtTimePlayed: number;
    appid: number;
}

export interface VoiceChatStatus {
    bVoiceChatActive: boolean;
    bMicMuted: boolean;
    bOutputMuted: boolean;
}
