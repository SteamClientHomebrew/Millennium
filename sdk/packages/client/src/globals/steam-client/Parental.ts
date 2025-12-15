import { EResult, Unregisterable } from "./shared";

/**
 * Interface for managing parental control settings.
 */
export interface Parental {
    /**
     * Locks the parental control settings.
     */
    LockParentalLock(): void;

    RegisterForParentalPlaytimeWarnings(callback: (time: number) => void): Unregisterable;

    /**
     * Registers a callback function to be invoked when parental settings change.
     * @param callback The callback function to be invoked when parental settings change.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForParentalSettingsChanges(callback: (settings: ParentalSettings) => void): Unregisterable;

    /**
     * Unlocks the parental lock with the provided PIN.
     * @param pin The 4-digit PIN to unlock the parental lock.
     * @returns a number representing the result of the unlock operation.
     */
    UnlockParentalLock(pin: string, param1: boolean): Promise<EResult>;
}

export interface ParentalSettings {
  ever_enabled: boolean;
  locked: boolean;
  /**
   * If deserialized, returns {@link ParentalSettingsProtoMsg}.
   */
  settings: ArrayBuffer;
  strPlaintextPassword: string;
}

/**
 * Represents the parental settings and restrictions.
 * @todo This whole thing is unconfirmed as I do not have access to parental
 * stuff and things
 */
export interface ParentalSettingsProtoMsg {
    steamid?: number;
    applist_base_id?: number;
    applist_base_description?: string;
    /**
     * Base list.
     */
    applist_base: ParentalApp[];
    /**
     * Custom list of allowed applications.
     */
    applist_custom: ParentalApp[];
    /**
     * @todo enum ?
     */
    passwordhashtype?: number;
    salt?: number;
    passwordhash?: number;
    /**
     * Indicates whether parental settings are enabled.
     */
    is_enabled?: boolean;
    /**
     * Bitmask representing enabled features.
     * - Bit 0: Unknown (@todo Please provide more details if known)
     * - Bit 1: Online content & features - Steam Store
     * - Bit 2: Online content & features - Community-generated content
     * - Bit 3: Online content & features - My online profile, screenshots, and achievements
     * - Bit 4: Online content & features - Friends, chat, and groups
     * - Bit 5-11: Unknown (@todo Please provide more details if known)
     * - Bit 12: Library content - 0: Only games I choose, 1: All games
     * @todo {@link EParentalFeature} ?
     */
    enabled_features?: number;
    /**
     * Email for recovery (if applicable).
     */
    recovery_email?: string;
    is_site_license_lock?: boolean;
    temporary_enabled_features?: number;
    rtime_temporary_feature_expiration?: number;
    playtime_restrictions?: ParentalPlaytimeRestrictions;
    temporary_playtime_restrictions?: ParentalTemporaryPlaytimeRestrictions;
    excluded_store_content_descriptors: number[];
    excluded_community_content_descriptors: number[];
    utility_appids: number[];
}

interface ParentalApp {
  appid: number;
  is_allowed: boolean;
}

interface ParentalPlaytimeDay {
  allowed_time_windows?: number;
  allowed_daily_minutes?: number;
}

interface ParentalPlaytimeRestrictions {
  apply_playtime_restrictions?: boolean;
  playtime_days: ParentalPlaytimeDay[];
}

interface ParentalTemporaryPlaytimeRestrictions {
  restrictions?: ParentalPlaytimeDay;
  rtime_expires?: number;
}

export enum EParentalFeature {
  Invalid,
  Store,
  Community,
  Profile,
  Friends,
  News,
  Trading,
  Settings,
  Console,
  Browser,
  ParentalSetup,
  Library,
  Test,
  SiteLicense,
  KioskMode,
  Max,
}
