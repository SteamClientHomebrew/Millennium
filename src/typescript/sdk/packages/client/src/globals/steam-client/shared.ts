export enum EResult {
  OK = 1,
  Fail,
  NoConnection,
  InvalidPassword = 5,
  LoggedInElsewhere,
  InvalidProtocolVer,
  InvalidParam,
  FileNotFound,
  Busy,
  InvalidState,
  InvalidName,
  InvalidEmail,
  DuplicateName,
  AccessDenied,
  Timeout,
  Banned,
  AccountNotFound,
  InvalidSteamID,
  ServiceUnavailable,
  NotLoggedOn,
  Pending,
  EncryptionFailure,
  InsufficientPrivilege,
  LimitExceeded,
  Revoked,
  Expired,
  AlreadyRedeemed,
  DuplicateRequest,
  AlreadyOwned,
  IPNotFound,
  PersistFailed,
  LockingFailed,
  LogonSessionReplaced,
  ConnectFailed,
  HandshakeFailed,
  IOFailure,
  RemoteDisconnect,
  ShoppingCartNotFound,
  Blocked,
  Ignored,
  NoMatch,
  AccountDisabled,
  ServiceReadOnly,
  AccountNotFeatured,
  AdministratorOK,
  ContentVersion,
  TryAnotherCM,
  PasswordRequiredToKickSession,
  AlreadyLoggedInElsewhere,
  Suspended,
  Cancelled,
  DataCorruption,
  DiskFull,
  RemoteCallFailed,
  PasswordUnset,
  ExternalAccountUnlinked,
  PSNTicketInvalid,
  ExternalAccountAlreadyLinked,
  RemoteFileConflict,
  IllegalPassword,
  SameAsPreviousValue,
  AccountLogonDenied,
  CannotUseOldPassword,
  InvalidLoginAuthCode,
  AccountLogonDeniedNoMail,
  HardwareNotCapableOfIPT,
  IPTInitError,
  ParentalControlRestricted,
  FacebookQueryError,
  ExpiredLoginAuthCode,
  IPLoginRestrictionFailed,
  AccountLockedDown,
  AccountLogonDeniedVerifiedEmailRequired,
  NoMatchingURL,
  BadResponse,
  RequirePasswordReEntry,
  ValueOutOfRange,
  UnexpectedError,
  Disabled,
  InvalidCEGSubmission,
  RestrictedDevice,
  RegionLocked,
  RateLimitExceeded,
  AccountLoginDeniedNeedTwoFactor,
  ItemDeleted,
  AccountLoginDeniedThrottle,
  TwoFactorCodeMismatch,
  TwoFactorActivationCodeMismatch,
  AccountAssociatedToMultiplePartners,
  NotModified,
  NoMobileDevice,
  TimeNotSynced,
  SmsCodeFailed,
  AccountLimitExceeded,
  AccountActivityLimitExceeded,
  PhoneActivityLimitExceeded,
  RefundToWallet,
  EmailSendFailure,
  NotSettled,
  NeedCaptcha,
  GSLTDenied,
  GSOwnerDenied,
  InvalidItemType,
  IPBanned,
  GSLTExpired,
  InsufficientFunds,
  TooManyPending,
  NoSiteLicensesFound,
  WGNetworkSendExceeded,
  AccountNotFriends,
  LimitedUserAccount,
}

export enum EBrowserType {
  /**
   * No window is created (like SharedJSContext).
   */
  OffScreen,
  OpenVROverlay,
  OpenVROverlay_Dashboard,
  /**
   * A normal window.
   */
  DirectHWND,
  /**
   * A borderless window.
   */
  DirectHWND_Borderless,
  /**
   * An initially hidden window.
   * May be shown with {@link SteamClient.Window.ShowWindow}.
   */
  DirectHWND_Hidden,
  ChildHWNDNative,
  Offscreen_SteamUI = 12,
  OpenVROverlay_Subview,
}

export enum ESteamRealm {
  Unknown,
  Global,
  China,
}

/**
 * Controls how Gamescope renders the GamepadUI window when a game is running.
 */
export enum EUIComposition {
  /** Steam is not rendered on the screen. */
  Hidden,

  /**
   * Transparent divs will allow pixels from the app behind Steam to penetrate.
   * Input goes to **the app**.
   */
  Notification,

  /**
   * Transparent divs will allow pixels from the app behind Steam to penetrate.
   * Input goes to **Steam**.
   */
  Overlay,

  /** Take all of the pixels on the screen, nothing "behind" Steam is shown. */
  Opaque,

  /**
   * Special composition mode that matches {@link Overlay}, but forwards synthetic keyboard
   * events to the Gamescope foreground app (game) instead of Steam.
   */
  OverlayKeyboard,
}

export enum EUIMode {
  Unknown = -1,
  GamePad = 4,
  Desktop = 7,
}

/** 0 - false, 1 - true */
export type VDFBoolean_t = 0 | 1;

export interface BrowserContext {
  /**
   * Window type.
   */
  m_eBrowserType?: EBrowserType;

  /**
   * The UI mode in use.
   */
  m_eUIMode?: EUIMode;

  /**
   * @todo Appears when {@link m_eBrowserType} == 0 ?
   */
  m_gameID?: string;

  /**
   * @todo Same as `SteamClient.Browser.GetBrowserID()` ?
   */
  m_nBrowserID: number;

  /**
   * Game's app ID.
   */
  m_unAppID?: number;

  /**
   * If overlay, game's PID.
   */
  m_unPID: number;
}

/**
 * Represents the response of an operation. It appears to be not necessary to await for this operation response. It is only used to indicate the result of an operation.
 */
export interface OperationResponse {
  /**
   * The result code of the operation.
   */
  result: EResult;

  /**
   * An unlocalized message describing the result of the operation.
   */
  message: string;
}

export interface Unregisterable {
  /**
   * Unregister the callback.
   */
  unregister(): void;
}

/**
 * ProtoBuf message class.
 */
export interface JsPbMessageClass {
  /**
   * @todo Returns {@link JsPbMessage}, but not sure how to do it for the messages.
   */
  deserializeBinary(data: ArrayBuffer): any;
}

/**
 * Deserialized ProtoBuf message.
 */
export interface JsPbMessage {
  getClassName(): string;

  serializeBase64String(): string;

  serializeBinary(): Uint8Array;

  /**
   * Converts the message to an object.
   */
  toObject(includeJsPbInstance: boolean): any;
}
