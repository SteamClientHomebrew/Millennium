import { JsPbMessage, OperationResponse, Unregisterable } from "../../shared";
import {Device} from "./Device";

export interface Network {
    Device: Device;

    ForceRefresh(): Promise<OperationResponse>;

    ForceTestConnectivity(): void;

    GetProxyInfo(): Promise<ProxyInfo>;

    RegisterForAppSummaryUpdate(callback: (data: AppSummary) => void): Unregisterable;

    /**
     * If `state` is deserialized, returns {@link CGameNetworkingUI_ConnectionState}.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForConnectionStateUpdate(callback: (state: ArrayBuffer) => void): Unregisterable;

    RegisterForConnectivityTestChanges(callback: (connectivityTest: ConnectivityTest) => void): Unregisterable;

    /**
     * If `data` is deserialized, returns {@link CMsgNetworkDevicesData}.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForDeviceChanges(callback: (data: ArrayBuffer) => void): Unregisterable;

    SetFakeLocalSystemState(state: ENetFakeLocalSystemState): void;

    SetProxyInfo(mode: number, address: string, port: number, excludeLocal: boolean): void;

    SetWifiEnabled(value: boolean): Promise<OperationResponse>;

    StartScanningForNetworks(): Promise<OperationResponse>;

    StopScanningForNetworks(): Promise<OperationResponse>;
}

export interface ConnectivityTest {
    eConnectivityTestResult: EConnectivityTestResult;
    eFakeState: ENetFakeLocalSystemState;
    bChecking: boolean;
}

export enum EConnectivityTestResult {
    Unknown,
    Connected,
    CaptivePortal,
    TimedOut,
    Failed,
    WifiDisabled,
    NoLAN,
}

export interface ProxyInfo {
    proxy_mode: number;
    address: string;
    port: number;
    exclude_local: boolean;
}

interface AppSummary {
    nAppID: number;
    /**
     * If deserialized, returns {@link CGameNetworkingUI_AppSummary}.
     */
    serializedMessage: ArrayBuffer;
}

export enum ENetFakeLocalSystemState {
    Normal,
    NoLAN,
    CaptivePortal_Redirected,
    CaptivePortal_InPlace,
    NoInternet,
    NoSteam,
}

export interface CMsgNetworkDevicesData extends JsPbMessage {
    devices(): NetworkDevice[];

    is_wifi_enabled(): boolean;

    is_wifi_scanning_enabled(): boolean;
}

export interface CGameNetworkingUI_ConnectionSummary {
    transport_kind?: number;
    connection_state?: number;
    sdrpop_local?: number;
    sdrpop_remote?: number;
    ping_ms?: number;
    packet_loss?: number;
    ping_default_internet_route?: number;
    ip_was_shared?: number;
}

export interface CGameNetworkingUI_AppSummary {
    appid?: number;
    ip_was_shared_with_friend?: boolean;
    ip_was_shared_with_nonfriend?: boolean;
    active_connections?: number;
    main_cxn?: CGameNetworkingUI_ConnectionSummary;
}

export interface SteamDatagramLinkInstantaneousStats {
    out_packets_per_sec_x10: number | undefined;
    out_bytes_per_sec: number | undefined;
    in_packets_per_sec_x10: number | undefined;
    in_bytes_per_sec: number | undefined;
    ping_ms: number | undefined;
    packets_dropped_pct: number | undefined;
    packets_weird_sequence_pct: number | undefined;
    peak_jitter_usec: number | undefined;
}

export interface SteamDatagramLinkLifetimeStats {
    connected_seconds: number | undefined;
    packets_sent: number | undefined;
    kb_sent: number | undefined;
    packets_recv: number | undefined;
    kb_recv: number | undefined;
    packets_recv_sequenced: number | undefined;
    packets_recv_dropped: number | undefined;
    packets_recv_out_of_order: number | undefined;
    packets_recv_out_of_order_corrected: number | undefined;
    packets_recv_duplicate: number | undefined;
    packets_recv_lurch: number | undefined;
    multipath_packets_recv_sequenced: number[];
    multipath_packets_recv_later: number[];
    multipath_send_enabled: number | undefined;
    quality_histogram_100: number | undefined;
    quality_histogram_99: number | undefined;
    quality_histogram_97: number | undefined;
    quality_histogram_95: number | undefined;
    quality_histogram_90: number | undefined;
    quality_histogram_75: number | undefined;
    quality_histogram_50: number | undefined;
    quality_histogram_1: number | undefined;
    quality_histogram_dead: number | undefined;
    quality_ntile_2nd: number | undefined;
    quality_ntile_5th: number | undefined;
    quality_ntile_25th: number | undefined;
    quality_ntile_50th: number | undefined;
    ping_histogram_25: number | undefined;
    ping_histogram_50: number | undefined;
    ping_histogram_75: number | undefined;
    ping_histogram_100: number | undefined;
    ping_histogram_125: number | undefined;
    ping_histogram_150: number | undefined;
    ping_histogram_200: number | undefined;
    ping_histogram_300: number | undefined;
    ping_histogram_max: number | undefined;
    ping_ntile_5th: number | undefined;
    ping_ntile_50th: number | undefined;
    ping_ntile_75th: number | undefined;
    ping_ntile_95th: number | undefined;
    ping_ntile_98th: number | undefined;
    jitter_histogram_negligible: number | undefined;
    jitter_histogram_1: number | undefined;
    jitter_histogram_2: number | undefined;
    jitter_histogram_5: number | undefined;
    jitter_histogram_10: number | undefined;
    jitter_histogram_20: number | undefined;
    txspeed_max: number | undefined;
    txspeed_histogram_16: number | undefined;
    txspeed_histogram_32: number | undefined;
    txspeed_histogram_64: number | undefined;
    txspeed_histogram_128: number | undefined;
    txspeed_histogram_256: number | undefined;
    txspeed_histogram_512: number | undefined;
    txspeed_histogram_1024: number | undefined;
    txspeed_histogram_max: number | undefined;
    txspeed_ntile_5th: number | undefined;
    txspeed_ntile_50th: number | undefined;
    txspeed_ntile_75th: number | undefined;
    txspeed_ntile_95th: number | undefined;
    txspeed_ntile_98th: number | undefined;
    rxspeed_max: number | undefined;
    rxspeed_histogram_16: number | undefined;
    rxspeed_histogram_32: number | undefined;
    rxspeed_histogram_64: number | undefined;
    rxspeed_histogram_128: number | undefined;
    rxspeed_histogram_256: number | undefined;
    rxspeed_histogram_512: number | undefined;
    rxspeed_histogram_1024: number | undefined;
    rxspeed_histogram_max: number | undefined;
    rxspeed_ntile_5th: number | undefined;
    rxspeed_ntile_50th: number | undefined;
    rxspeed_ntile_75th: number | undefined;
    rxspeed_ntile_95th: number | undefined;
    rxspeed_ntile_98th: number | undefined;
}

export interface SteamDatagramConnectionQuality {
    instantaneous: SteamDatagramLinkInstantaneousStats | undefined;
    lifetime: SteamDatagramLinkLifetimeStats | undefined;
}

export interface SteamNetworkingICESessionSummary {
    failure_reason_code: number | undefined;
    local_candidate_types: number | undefined;
    remote_candidate_types: number | undefined;
    initial_route_kind: number | undefined;
    initial_ping: number | undefined;
    initial_score: number | undefined;
    negotiation_ms: number | undefined;
    best_route_kind: number | undefined;
    best_ping: number | undefined;
    best_score: number | undefined;
    best_time: number | undefined;
    selected_seconds: number | undefined;
    user_settings: number | undefined;
    ice_enable_var: number | undefined;
    local_candidate_types_allowed: number | undefined;
}

export interface SteamNetworkingP2PSDRRoutingSummary {
    initial_ping: number | undefined;
    initial_ping_front_local: number | undefined;
    initial_ping_front_remote: number | undefined;
    initial_score: number | undefined;
    initial_pop_local: number | undefined;
    initial_pop_remote: number | undefined;
    best_ping: number | undefined;
    best_ping_front_local: number | undefined;
    best_ping_front_remote: number | undefined;
    best_score: number | undefined;
    best_pop_local: number | undefined;
    best_pop_remote: number | undefined;
    best_time: number | undefined;
    negotiation_ms: number | undefined;
    selected_seconds: number | undefined;
}

export interface SteamDatagramP2PRoutingSummary {
    ice: SteamNetworkingICESessionSummary | undefined;
    sdr: SteamNetworkingP2PSDRRoutingSummary | undefined;
}

export interface CGameNetworkingUI_ConnectionState extends JsPbMessage {
    connection_key(): string;

    appid(): number;

    connection_id_local(): number;

    identity_local(): string;

    identity_remote(): string;

    connection_state(): number;

    start_time(): number;

    close_time(): number;

    close_reason(): number;

    close_message(): string;

    status_loc_token(): string;

    transport_kind(): number;

    sdrpopid_local(): string;

    sdrpopid_remote(): string;

    address_remote(): string;

    p2p_routing(): SteamDatagramP2PRoutingSummary;

    ping_interior(): number;

    ping_remote_front(): number;

    ping_default_internet_route(): number;

    e2e_quality_local(): SteamDatagramConnectionQuality;

    e2e_quality_remote(): SteamDatagramConnectionQuality;

    e2e_quality_remote_instantaneous_time(): string;

    e2e_quality_remote_lifetime_time(): string;

    front_quality_local(): SteamDatagramConnectionQuality;

    front_quality_remote(): SteamDatagramConnectionQuality;

    front_quality_remote_instantaneous_time(): string;

    front_quality_remote_lifetime_time(): string;
}

export interface NetworkDevice_Wireless {
    aps: WirelessAP[];
    /**
     * @remarks Not present if wired.
     * @todo enum
     */
    esecurity_supported?: number;
}

export interface WirelessAP {
    esecurity: EWirelessSecurityFlags;
    estrength: EWirelessEndpointStrength;
    id: number;
    is_active: boolean;
    is_autoconnect: boolean;
    password: string;
    ssid: string;
    strength_raw: number;
    user_name?: string;
}

export enum EWirelessSecurityFlags {
    None,
    StaticWep = 1 << 0,
    DynamicWep = 1 << 1,
    Wpa = 1 << 2,
    WpaEnterprise = 1 << 3,
    Wpa2 = 1 << 4,
    Wpa2Enterprise = 1 << 5,
    /**
     * Special value to indicate that this platform does not support
     * the security methods required to connect to an access point
     */
    Unsupported = 1 << 15,
}

export interface NetworkDevice_Wired {
    friendly_name: string;
    is_cable_present: boolean;
    speed_mbit: number;
}

export interface NetworkDevice {
    estate: ENetworkDeviceState;
    etype: ENetworkDeviceType;
    id: number;
    ipv4: {
        addresses: NetworkDeviceIPv4Address[];
    };
    ipv6: {
        addresses: NetworkDeviceIPv6Address[];
    };
    mac: string;
    product: string;
    vendor: string;
    /**
     * @remarks Present only if wired.
     */
    wired?: NetworkDevice_Wired;
    /**
     * @remarks Present even if wired.
     */
    wireless: NetworkDevice_Wireless;
}

export interface NetworkDeviceIPv4Address {
    ip: number;
    netmask: number;
}

export interface NetworkDeviceIPv6Address {
    ip: string;
}

export interface NetworkDeviceIP {
    dns_ip: number[];
    gateway_ip: number;
    is_default_route: boolean;
    is_dhcp_enabled: boolean;
    is_enabled: boolean;
}

export enum ENetworkDeviceState {
    NotPresent,
    Failed,
    Disconnected,
    Disconnecting,
    Connecting,
    Connected,
    Retrying,
}

export enum ENetworkDeviceType {
    Unknown,
    Wired,
    Wireless,
    Virtual,
}

export enum EWirelessEndpointStrength {
    None,
    Weak,
    Ok,
    Good,
    Excellent,
}
