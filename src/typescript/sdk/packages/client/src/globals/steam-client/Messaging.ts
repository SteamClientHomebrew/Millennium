import { Unregisterable } from "./shared";

export interface Messaging {
    // section - "ContentManagement", "JumpList", "PostToLibrary"
    // seems multipurpose
    RegisterForMessages<T extends string>(
        message: T,
        callback: (message: T, section: string, args: string) => void,
    ): Unregisterable;

    /*
    function m(e) {
        SteamClient.Messaging.PostMessage("LibraryCommands", "ShowFriendChatDialog", JSON.stringify({
            steamid: e.persona.m_steamid.ConvertTo64BitString()
        }))
    }
    SteamClient.Messaging.PostMessage("FriendsUI", "AcceptedRemotePlayInvite", JSON.stringify({id: this.appID})) : SteamClient.Messaging.PostMessage("FriendsUI", "AcceptedGameInvite", JSON.stringify({id: this.appID}))
     */
    PostMessage(message: string, section: string, args: string): void;
}
