import { Unregisterable } from "../shared";
import {BrowserViewPopup} from "./BrowserViewPopup";

export interface BrowserView {
    /**
     * @note Not available on a created BrowserView.
     */
    Create(options?: BrowserViewCreateOptions): BrowserViewPopup;

    /**
     * Like {@link Create}, but:
     *
     * - Lets you create a BrowserView with
     * `window.open()` instead, while still letting you control the BrowserView
     * the same way.
     *
     * @note Not available on a created BrowserView.
     */
    CreatePopup(options?: BrowserViewCreateOptions): {
        /**
         * URL to use with `window.open()`.
         */
        strCreateURL: string;
        browserView: BrowserViewPopup;
    };

    /**
     * @note Not available on a created BrowserView.
     */
    Destroy(browserView: BrowserViewPopup): void;

    /**
     * @note Only works on a created BrowserView.
     */
    PostMessageToParent(message: string, args: string): void;

    /**
     * Register a callback to be called when a message gets sent with
     * {@link BrowserViewPopup.PostMessage}.
     *
     * @note Only available on a created BrowserView.
     */
    RegisterForMessageFromParent(callback: (message: string, args: string) => void): Unregisterable;
}

export interface BrowserViewCreateOptions {
    bOnlyAllowTrustedPopups?: boolean;
    parentPopupBrowserID?: number;
    /** Initial URL to load. */
    strInitialURL?: string;
    strUserAgentIdentifier?: string;
    strUserAgentOverride?: string;
    strVROverlayKey?: string;
}
