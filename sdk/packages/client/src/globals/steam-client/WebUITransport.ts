import { OperationResponse } from "./shared";

export interface WebUITransport {
    GetTransportInfo(): Promise<TransportInfo>;

    /**
     * Tells Steam the websocket failed and opens a troubleshooting dialog.
     *
     * The responsible message for this is `CMsgWebUITransportFailure`.
     *
     * @param base64 Serialized ProtoBuf message.
     */
    NotifyTransportFailure(base64: string): Promise<OperationResponse>;
}

export interface TransportInfo {
    authKeyClientdll: string;
    authKeySteamUI: string;
    portClientdll: number;
    portSteamUI: number;
}
