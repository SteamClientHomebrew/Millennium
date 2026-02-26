import { Unregisterable } from "./shared";

export interface URL {
    /**
     * Executes a steam:// URL.
     * @param url The URL to execute.
     */
    ExecuteSteamURL(url: string): void;

    /**
     * @param urls Additional URLs to get. May be empty.
     */
    GetSteamURLList(urls: SteamWebURL_t[]): Promise<SteamURLs>;

    GetWebSessionID(): Promise<string>;

    /**
     * Registers a callback to be called when a steam:// URL gets executed.
     * @param section `rungameid`, `open`, etc.
     * @param callback The callback function to be called.
     * @returns an object that can be used to unregister the callback.
     */
    RegisterForRunSteamURL(section: string, callback: (param0: number, url: string) => void): Unregisterable;

    RegisterForSteamURLChanges(callback: () => void): void;
}

export type SteamWebURL_t =
    | "AllNotifications"
    | "AppHoverPublic"
    | "AppHoverPublicFull"
    | "AppNewsPage"
    | "AsyncGames"
    | "AvatarBaseURL"
    | "BaseURLSharedCDN"
    | "Chat"
    | "ChatRoot"
    | "ClaimEntitlements"
    | "ClanAssetCDN"
    | "CommentNotifications"
    | "CommunityHome"
    | "CommunityAddFriends"
    | "CommunityCDN"
    | "CommunityFilePage"
    | "CommunityFriendsThatPlay"
    | "CommunityFrontPage"
    | "CommunityGroupSearch"
    | "CommunityImages"
    | "CommunityInventory"
    | "CommunityMarket"
    | "CommunityMarketApp"
    | "CommunityRecommendations"
    | "CommunityScreenshots"
    | "CommunitySingleScreenshot"
    | "CurrentlyPlayedWith"
    | "EventAnnouncementPage"
    | "FamilyManagement"
    | "FamilySharing"
    | "GameHub"
    | "GameHubBroadcasts"
    | "GameHubDiscussions"
    | "GameHubGuides"
    | "GameHubNews"
    | "GameHubReviews"
    | "GlobalAchievementStatsPage"
    | "GlobalLeaderboardsPage"
    | "GroupSteamIDPage"
    | "HardwareSurvey"
    | "HelpAppPage"
    | "HelpChangeEmail"
    | "HelpChangePassword"
    | "HelpFAQ"
    | "HelpFrontPage"
    | "HelpWithLogin"
    | "HelpWithLoginInfo"
    | "HelpWithSteamGuardCode"
    | "HelpVacBans"
    | "ItemStorePage"
    | "ItemStoreDetailPage"
    | "JoinTrade"
    | "LegalInformation"
    | "LibraryAppDetails"
    | "LibraryAppReview"
    | "LibraryFeaturedBroadcasts"
    | "ManageGiftsPage"
    | "ManageSteamGuard"
    | "ModeratorMessages"
    | "Mobile"
    | "MyHelpRequests"
    | "OfficialGameGroupPage"
    | "NewsHomePage"
    | "ParentalBlocked"
    | "ParentalSetup"
    | "PendingFriends"
    | "PendingGift"
    | "PointsShop"
    | "PrivacyPolicy"
    | "RecommendGame"
    | "RedeemWalletVoucher"
    | "RegisterKey"
    | "RegisterKeyNoParams"
    | "SSA"
    | "SteamAnnouncements"
    | "SteamClientBetaBugReports"
    | "SteamClientBetaNewsPage"
    | "SteamClientBetaNewsPageFancy"
    | "SteamClientNewsPage"
    | "SteamClientPatchNotes"
    | "SteamClientBetaPatchNotes"
    | "SteamDiscussions"
    | "SteamIDAchievementsPage"
    | "SteamIDAppTradingCardsPage"
    | "SteamIDBadgeInfo"
    | "SteamIDBadgePage"
    | "SteamIDBroadcastPage"
    | "SteamIDEditPage"
    | "SteamIDEditPrivacyPage"
    | "SteamIDFriendsList"
    | "SteamIDFriendsPage"
    | "SteamIDGroupsPage"
    | "SteamIDMyProfile"
    | "SteamIDPage"
    | "SteamLanguage"
    | "SteamPreferences"
    | "SteamVRHMDHelp"
    | "SteamWorkshop"
    | "SteamWorkshopPage"
    | "SteamWorkshopSubscriptions"
    | "SteamWorkshopUpdatedSubscriptions"
    | "StoreAccount"
    | "StoreAddFundsPage"
    | "StoreAppHover"
    | "StoreAppImages"
    | "StoreAppPage"
    | "StoreAppPageAddToCart"
    | "StoreCart"
    | "StoreCDN"
    | "StoreDlcPage"
    | "StoreExplore"
    | "StoreExploreNew"
    | "StoreFreeToPlay"
    | "StoreFrontPage"
    | "StoreGameSearchPage"
    | "StoreGreatOnDeck"
    | "StorePublisherPage"
    | "StoreSpecials"
    | "StoreStats"
    | "StoreVR"
    | "StoreWebMicroTxnPage"
    | "SupportMessages"
    | "TextFilterSettings"
    | "TodayPage"
    | "TradeOffers"
    | "VideoCDN"
    | "UserAchievementsPage"
    | "UserLeaderboardsPage"
    | "UserStatsPage"
    | "UserWishlist"
    | "WatchVideo"
    | "WebAPI"
    | "WorkshopEula"
    | "YearInReview";

export interface SteamURL {
    url: string;
    /**
     * @todo enum?
     */
    feature: number;
}

export type SteamURLs = {
    [url in SteamWebURL_t]: SteamURL;
}
