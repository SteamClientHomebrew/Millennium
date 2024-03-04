#include "sdk.hpp"

const char* GET_SDK_REF()
{
	return R"(

console.log("Injecting Millennium SDK Reference")

const OptionsToString = (opts, char) =>
    Object.entries(opts)
    .map((e) => e.join("="))
    .join(char);
window.Millennium = {
    Utils: {
        /**
         * Waits for an HTML element.
         * @param {string} selector CSS selector.
         * @param {Node|Document} parent  Element parent.
         * @returns {Promise<Node>}
         */
        async WaitForElement(selector, parent = document) {
            return new Promise((resolve) => {
                const el = parent.querySelector(selector);
                if (el) {
                    resolve(el);
                }
                const observer = new MutationObserver(() => {
                    const el = parent.querySelector(selector);
                    if (!el) {
                        return;
                    }
                    resolve(el);
                    observer.disconnect();
                });
                observer.observe(document.body, {
                    subtree: true,
                    childList: true,
                });
            });
        },
        /**
         * Waits for a message, optionally for a specific window.
         *
         * @param {string} msg
         * @param {Window} wnd
         * @returns {Promise<void>} Resolves when the message is sent.
         */
        async WaitForMessage(msg, wnd = window) {
            return new Promise((resolve) => {
                function OnMessage(ev) {
                    if ((ev.data.message || ev.data) != msg) {
                        return;
                    }
                    resolve();
                    wnd.removeEventListener("message", OnMessage);
                }
                wnd.addEventListener("message", OnMessage);
            });
        },
    },
    Window: {
        /**
         * A `window.open()` wrapper for usage with Steam-specific window features.
         * @param options Steam-specific window features.
         * @param dimensions Window dimensions.
         * @returns {Promise<Window>} A Promise that resolves to the opened window.
         */
        async Create(options, dimensions) {
            const opener = window.opener || window;
            const wnd = opener.window.open(
                `about:blank?${OptionsToString(options, "&")}`,
                undefined,
                OptionsToString(
                    Object.assign(dimensions, {
                        status: false,
                        toolbar: false,
                        menubar: false,
                        location: false,
                    }),
                    ",",
                ),
            );
            await WaitForMessage("popup-created", wnd);
            return wnd;
        },
        Type: {
            OFFSCREEN: 0,
            OPENVR_OVERLAY: 1,
            OPENVR_OVERLAY_DASHBOARD: 4,
            DIRECT_HWND: 3,
            DIRECT_HWND_BORDERLESS: 5,
            DIRECT_HWND_HIDDEN: 6,
            CHILD_HWND_NATIVE: 7,
            TRANSPARENT_TOPLEVEL: 8,
            OFFSCREEN_SHARED_TEXTURE: 9,
            OFFSCREEN_GAME_OVERLAY: 10,
            OFFSCREEN_GAME_OVERLAY_SHARED_TEXTURE: 15,
            OFFSCREEN_FRIENDS_UI: 12,
            OFFSCREEN_STEAM_UI: 13,
            OPENVR_OVERLAY_SUBVIEW: 14,
        },
        CreationFlags: {
            NONE: 0,
            MINIMIZED: 1 << 0,
            HIDDEN: 1 << 1,
            TOOLTIP_HINT: 1 << 2,
            NO_TASKBAR_ICON: 1 << 3,
            RESIZABLE: 1 << 4,
            SCALE_POSITION: 1 << 5,
            SCALE_SIZE: 1 << 6,
            MAXIMIZED: 1 << 7,
            BACKGROUND_TRANSPARENT: 1 << 8,
            NOT_FOCUSABLE: 1 << 9,
            FULLSCREEN: 1 << 10,
            FULLSCREEN_EXCLUSIVE: 1 << 11,
            APPLY_BROWSER_SCALE_TO_DIMENSIONS: 1 << 12,
            ALWAYS_ON_TOP: 1 << 13,
            NO_WINDOW_SHADOW: 1 << 14,
            NO_MINIMIZE: 1 << 15,
            POP_UP_MENU_HINT: 1 << 16,
            IGNORE_SAVED_SIZE: 1 << 17,
            NO_ROUNDED_CORNERS: 1 << 18,
            FORCE_ROUNDED_CORNERS: 1 << 19,
            OVERRIDE_REDIRECT: 1 << 20,
            IGNORE_STEAM_DISPLAY_SCALE: 1 << 21,
        },
    },
};
)";}
