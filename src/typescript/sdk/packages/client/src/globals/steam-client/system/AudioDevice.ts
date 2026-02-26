import { JsPbMessage, Unregisterable } from "../shared";

export interface AudioDevice {
    /**
     * If `data` is deserialized, returns {@link CMsgSystemAudioManagerState}.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStateChanges(callback: (data: ArrayBuffer) => void): Unregisterable;

    /**
     * @param base64 Serialized base64 message from `CMsgSystemAudioManagerUpdateSomething`.
     */
    UpdateSomething(base64: string): void;
}

export interface CMsgSystemAudioManagerState extends JsPbMessage {
    counter(): number | undefined;

    hw(): MsgSystemAudioManagerStateHW | undefined;

    rtime_filter(): number | undefined;
}

export interface MsgSystemAudioManagerStateHW {
    devices: MsgSystemAudioManagerDevice[];
    nodes: MsgSystemAudioManagerNode[];
    ports: MsgSystemAudioManagerPort[];
    links: MsgSystemAudioManagerLink[];
}

export interface MsgSystemAudioManagerDevice {
    base: MsgSystemAudioManagerObject | undefined;
    name: string | undefined;
    nick: string | undefined;
    description: string | undefined;
    api: string | undefined;
}

export interface MsgSystemAudioManagerNode {
    base: MsgSystemAudioManagerObject | undefined;
    device_id: number | undefined;
    name: string | undefined;
    nick: string | undefined;
    description: string | undefined;
    edirection: ESystemAudioDirection | undefined;
    volume: MsgSystemAudioVolume | undefined;
}

export interface MsgSystemAudioManagerPort {
    base: MsgSystemAudioManagerObject | undefined;
    node_id: number | undefined;
    name: string | undefined;
    alias: string | undefined;
    etype: ESystemAudioPortType | undefined;
    edirection: ESystemAudioPortDirection | undefined;
    is_physical: boolean | undefined;
    is_terminal: boolean | undefined;
    is_control: boolean | undefined;
    is_monitor: boolean | undefined;
}

export interface MsgSystemAudioVolume {
    entries: MsgSystemAudioVolumeChannelEntry[] | undefined;
    is_muted: boolean | undefined;
}

export interface MsgSystemAudioVolumeChannelEntry {
    echannel: ESystemAudioChannel | undefined;
    volume: number | undefined;
}

export interface MsgSystemAudioManagerLink {
    base: MsgSystemAudioManagerObject | undefined;
    output_node_id: number | undefined;
    output_port_id: number | undefined;
    input_node_id: number | undefined;
    input_port_id: number | undefined;
}

export interface MsgSystemAudioManagerObject {
    id: number | undefined;
    rtime_last_update: number | undefined;
}

export enum ESystemAudioDirection {
    Invalid,
    Input,
    Output,
}

export enum ESystemAudioPortDirection {
    Invalid,
    Input,
    Output,
}

export enum ESystemAudioPortType {
    Invalid,
    Unknown,
    Audio32f,
    Midi8b,
    Video32RGBA,
}

export enum ESystemAudioChannel {
    Invalid,
    Aggregated,
    FrontLeft,
    FrontRight,
    LFE,
    BackLeft,
    BackRight,
    FrontCenter,
    Unknown,
    Mono,
}
