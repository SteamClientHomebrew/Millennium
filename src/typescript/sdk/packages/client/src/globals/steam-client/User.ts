import { EResult, OperationResponse, Unregisterable } from "./shared";

export interface User {
    AuthorizeMicrotxn(txnId: number | string): void;

    CancelLogin(): void;

    CancelMicrotxn(txnId: number | string): void;

    /**
     * Tries to cancel Steam shutdown.
     * @remarks Used in the "Shutting down" dialog.
     */
    CancelShutdown(): void;

    /**
     * Opens the "Change Account" dialog.
     */
    ChangeUser(): void;

    Connect(): Promise<OperationResponse>;

    FlipToLogin(): void;

    /**
     * Forces a shutdown while shutting down.
     * @remarks Used in the "Shutting down" dialog.
     */
    ForceShutdown(): void;

    /**
     * Forgets an account's password.
     * @param accountName Login of the account to forget.
     * @returns a boolean indicating whether the operation succeeded or not.
     */
    ForgetPassword(accountName: string): Promise<boolean>;

    /**
     * Gets your country code.
     * @returns a string containing your country code.
     */
    GetIPCountry(): Promise<string>;

    /**
     * @todo param0 mirrors param3 of {@link RegisterForLoginStateChange}
     */
    GetLoginProgress(callback: (param0: number, param1: number) => void): Unregisterable;

    GetLoginUsers(): Promise<LoginUser[]>;

    GoOffline(): void;

    GoOnline(): void;

    OptOutOfSurvey(): void;

    PrepareForSystemSuspend(): Promise<{
        result: EResult;
    }>;

    Reconnect(): void;

    RegisterForConnectionAttemptsThrottled(callback: (data: ConnectionAttempt) => void): Unregisterable;

    RegisterForCurrentUserChanges(callback: (user: CurrentUser) => void): void;

    RegisterForLoginStateChange(
        callback: (
            /**
             * Empty if not logged in.
             */
            accountName: string,
            state: ELoginState,
            result: EResult,
            param3: number,
            percentage: number,
            /**
             * @todo name is from CLoginStore, but it's always empty, unused ?
             */
            emailDomain: string,
        ) => void
    ): Unregisterable;

    RegisterForPrepareForSystemSuspendProgress(callback: (progress: SuspendProgress) => void): Unregisterable;

    RegisterForResumeSuspendedGamesProgress(callback: (progress: SuspendProgress) => void): Unregisterable;

    RegisterForShowHardwareSurvey(callback: () => void): Unregisterable;

    /**
     * Register a function to be executed when shutdown completes.
     * @param callback The function to be executed on completion.
     */
    RegisterForShutdownDone(callback: (state: EShutdownStep, appid: number, param2: boolean) => void): Unregisterable;

    RegisterForShutdownFailed(callback: (state: EShutdownStep, appid: number, success: boolean) => void): Unregisterable;

    /**
     * Register a function to be executed when Steam starts shutting down.
     * @param callback The function to be executed on shutdown start.
     */
    RegisterForShutdownStart(callback: (param0: boolean) => void): Unregisterable;

    /**
     * Register a function to be executed when shutdown state changes.
     * @param callback The function to be executed on change.
     */
    RegisterForShutdownState(callback: (state: EShutdownStep, appid: number, allowForceQuit: boolean) => void): Unregisterable;

    /**
     * Removes an account from remembered users.
     * @param accountName The account to remove.
     */
    RemoveUser(accountName: string): void;

    RequestSupportSystemReport(reportId: string): Promise<{
        bSuccess: boolean;
    }>;

    ResumeSuspendedGames(param0: boolean): Promise<ResumeSuspendedGamesResult>;

    // Hardware survey information
    RunSurvey(callback: (surveySections: SurveySection[]) => void): void;

    SendSurvey(): void;

    SetAsyncNotificationEnabled(appId: number, enable: boolean): void;

    /**
     * Sets given login credentials, but don't log in to that account.
     * @param accountName Account name.
     * @param password Account password.
     * @param rememberMe Whether to remember that account.
     */
    SetLoginCredentials(accountName: string, password: string, rememberMe: boolean): void;

    SetOOBEComplete(): void;

    ShouldShowUserChooser(): Promise<boolean>;

    /**
     * Signs out and restarts Steam.
     */
    SignOutAndRestart(): void;

    /**
     * Relogin after disabling offline mode. Not sure what else it's useful for,
     * there isn't even a single mention of it in steam's js, lol
     */
    StartLogin(): void;

    /**
     * Toggles offline mode.
     *
     * Note that after disabling offline mode, you have to relogin with
     * {@link StartLogin}.
     */
    StartOffline(value: boolean): void;

    /**
     * Restarts the Steam client.
     *
     * @todo I don't remember what the arg is, but IIRC with `true` it disables
     * some ldd check or whatever, really it's only noticeable on slow PCs.
     */
    StartRestart(force: boolean): void;

    /**
     * @todo I don't remember what the arg is, but IIRC with `true` it disables
     * some ldd check or whatever, really it's only noticeable on slow PCs.
     */
    StartShutdown(force: boolean): void;
}

export interface ConnectionAttempt {
    rtCooldownExpiration: number;
}

export interface CurrentUser {
    NotificationCounts: {
        async_game_updates: number;
        comments: number;
        gifts: number;
        help_request_replies: number;
        inventory_items: number;
        invites: number;
        moderator_messages: number;
        offline_messages: number;
        trade_offers: number;
    };
    bHWSurveyPending: boolean;
    bIsLimited: boolean;
    bIsOfflineMode: boolean;
    bPromptToChangePassword: boolean;
    bSupportAckOnlyMessages: boolean;
    bSupportAlertActive: boolean;
    bSupportPopupMessage: boolean;
    clientinstanceid: string;
    strAccountBalance: string;
    strAccountBalancePending: string;
    strAccountName: string;
    strFamilyGroupID: string;
    strSteamID: string;
}

export enum ELoginState {
    None,
    WelcomeDialog,
    WaitingForCreateUser,
    WaitingForCredentials,
    WaitingForNetwork,
    WaitingForServerResponse,
    WaitingForLibraryReady,
    Success,
    Quit,
}

export enum EShutdownStep {
  None,
  Start,
  WaitForGames,
  WaitForCloud,
  FinishingDownload,
  WaitForDownload,
  WaitForServiceApps,
  WaitForLogOff,
  Done,
  // TODO: RegisterForShutdownDone outputs 9 here
}

export enum ESuspendResumeProgressState {
  Invalid,
  Complete,
  CloudSync,
  LoggingIn,
  WaitingForApp,
  Working,
}

export interface LoginUser {
    personaName: string;
    accountName: string;
    hasPin: boolean;
    rememberPassword: boolean;
    avatarUrl: string;
}

export interface ResumeSuspendedGamesResult {
  nAppIDPlayingElsewhere: number;
  result: EResult;
}

export interface SuspendProgress {
  bGameSuspended: boolean;
  state: ESuspendResumeProgressState;
}

export interface SurveyEntry {
    strName: string;
    vecArgs: string[];
}

export interface SurveySection {
    strSectionName: string;
    vecEntries: SurveyEntry[];
}
