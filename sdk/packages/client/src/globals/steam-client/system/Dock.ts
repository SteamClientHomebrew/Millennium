import { JsPbMessage, Unregisterable } from "../shared";
import {EUpdaterState} from "../Updates";

export interface Dock {
    DisarmSafetyNet(): void;

    /**
     * If `data` is deserialized, returns {@link MsgSystemDockState}.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForStateChanges(callback: (data: ArrayBuffer) => void): Unregisterable;

    /**
     * @param base64 Serialized base64 message from `CMsgSystemDockUpdateFirmware`.
     */
    UpdateFirmware(base64: string): any;
}

/**
 * CMsgSystemDockState
 */
export interface MsgSystemDockState extends JsPbMessage {
    update_state(): SystemDockUpdateState | undefined;
}

export interface SystemDockUpdateState {
    state: EUpdaterState | undefined;
    rtime_last_checked: number | undefined;
    version_current: string | undefined;
    version_available: string | undefined;
    stage_progress: number | undefined;
    rtime_estimated_completion: number | undefined;
    old_fw_workaround: number | undefined;
}
