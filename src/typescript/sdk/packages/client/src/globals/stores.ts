import { WindowRouter } from '../modules/Router';
import { AppDetails, LogoPosition, SteamAppOverview } from "./steam-client/App";

interface AppData {
  details: AppDetails;
  // more
}

interface AppStoreAppOverview extends SteamAppOverview {
  m_setStoreCategories: Set<number>;
  m_setStoreTags: Set<number>;
  m_strPerClientData: Set<any> | undefined;
  m_strAssociations: Set<any> | undefined;

  BIsModOrShortcut: () => boolean;
  BIsShortcut: () => boolean;
}

declare global {
  interface Window {
    LocalizationManager: {
      m_mapTokens: Map<string, string>;
      m_mapFallbackTokens: Map<string, string>;
      m_rgLocalesToUse: string[];
    };
    App: {
      m_CurrentUser: {
        bIsLimited: boolean;
        bIsOfflineMode: boolean;
        bSupportAlertActive: boolean;
        bCanInviteFriends: boolean;
        NotificationCounts: {
          comments: number;
          inventory_items: number;
          invites: number;
          gifts: number;
          offline_messages: number;
          trade_offers: number;
          async_game_updates: number;
          moderator_messages: number;
          help_request_replies: number;
        };
        strAccountBalance: string;
        strAccountName: string;
        strSteamID: string;
      };
    };
    appStore: {
      GetAppOverviewByAppID: (appId: number) => SteamAppOverview | null;
      GetCustomVerticalCapsuleURLs: (app: AppStoreAppOverview) => string[];
      GetCustomLandcapeImageURLs: (app: AppStoreAppOverview) => string[];
      GetCustomHeroImageURLs: (app: AppStoreAppOverview) => string[];
      GetCustomLogoImageURLs: (app: AppStoreAppOverview) => string[];
      GetLandscapeImageURLForApp: (app: AppStoreAppOverview) => string;
      GetVerticalCapsuleURLForApp: (app: AppStoreAppOverview) => string;
      GetCachedLandscapeImageURLForApp: (app: AppStoreAppOverview) => string;
      GetCachedVerticalImageURLForApp: (app: AppStoreAppOverview) => string;
      GetPregeneratedVerticalCapsuleForApp: (app: AppStoreAppOverview) => string;
      GetIconURLForApp: (app: AppStoreAppOverview) => string;
    };
    appDetailsStore: {
      GetAppData: (appId: number) => AppData | null;
      GetAppDetails: (appId: number) => AppDetails | null;
      GetCustomLogoPosition: (app: AppStoreAppOverview) => LogoPosition | null;
      SaveCustomLogoPosition: (app: AppStoreAppOverview, logoPositions: LogoPosition) => any;
    };
    SteamUIStore: {
      GetFocusedWindowInstance: () => WindowRouter;
    };
    securitystore: {
      IsLockScreenActive: () => boolean;
    };
  }
}
