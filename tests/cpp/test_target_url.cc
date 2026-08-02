#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "millennium/target_url.h"

struct url_test_case {
    std::string description;
    std::string url;
    bool expected_network_safe;
    bool expected_js_safe;
    bool expected_webkit_injection;
};

TEST_CASE("WebKit Target URL Validation", "[webkit]")
{
    SECTION("Domain Suffix Matching (is_domain_match)") {
        // Exact matches
        REQUIRE(target_url::is_domain_match("paypal.com", "paypal.com"));
        REQUIRE(target_url::is_domain_match("youtube.com", "youtube.com"));

        // Valid subdomains
        REQUIRE(target_url::is_domain_match("www.paypal.com", "paypal.com"));
        REQUIRE(target_url::is_domain_match("checkout.paypal.com", "paypal.com"));
        REQUIRE(target_url::is_domain_match("a.b.c.paypal.com", "paypal.com"));

        // Invalid partial matches (Spoofing attempts)
        REQUIRE_FALSE(target_url::is_domain_match("fakepaypal.com", "paypal.com"));
        REQUIRE_FALSE(target_url::is_domain_match("paypal.com.evil.com", "paypal.com"));
        REQUIRE_FALSE(target_url::is_domain_match("mypaypal.com", "paypal.com"));
    }

    SECTION("Exhaustive URL Policies") {
        auto test_case = GENERATE(
            // Empty and Invalid URLs
            url_test_case{"Empty URL", "", false, false, false},
            url_test_case{"Invalid Scheme (ftp)", "ftp://store.steampowered.com", false, false, false},
            url_test_case{"Invalid Scheme (file)", "file:///C:/Users/test/index.html", false, false, false},
            url_test_case{"Invalid URL", "https://[::1]/", true, true, false},

            // Native Steam Domains (Auto-Allowed)
            url_test_case{"Storefront", "https://store.steampowered.com/", true, true, true},
            url_test_case{"Community", "https://steamcommunity.com/id/someone", true, true, true},
            url_test_case{"Steamgames HTTP", "http://steamgames.com/some/path", true, true, true},
            
            // Edge cases handled by libcurl
            url_test_case{"Storefront with port", "https://store.steampowered.com:443/", true, true, true},
            url_test_case{"Case-insensitive host", "https://STORE.STEAMPOWERED.COM/", true, true, true},
            url_test_case{"Case-insensitive scheme", "HTTPS://STORE.STEAMPOWERED.COM/", true, true, true},
            url_test_case{"Trailing dot", "https://store.steampowered.com./", true, true, true},
            url_test_case{"Basic auth", "https://user:pass@steamcommunity.com/", true, true, true},

            // Spoofed Native Steam Domains (Forbidden)
            url_test_case{"Fake host prefix", "https://fakesteampowered.com/", true, true, false}, // Network safe, JS safe, but not steam owned so no injection
            url_test_case{"Fake host suffix", "https://store.steampowered.com.evil.com/", true, true, false},

            // Main UI (steamloopback) Forbidden
            url_test_case{"Steam loopback", "https://steamloopback.host/index.html", true, true, false}, // Steam owned, but forbidden explicitly for injection

            // Steam Checkout Blacklist (JS Forbidden, CSS Allowed)
            url_test_case{"Steam checkout", "https://checkout.steampowered.com/pay", true, false, false},

            // Global Network Blacklists (JS and CSS Forbidden)
            url_test_case{"PayPal", "https://www.paypal.com/checkout", false, false, false},
            url_test_case{"PayPal objects", "https://www.paypalobjects.com/webstatic/mktg/logo/pp_cc_mark_111x69.jpg", false, false, false},
            url_test_case{"Recaptcha", "https://www.recaptcha.net/recaptcha/api.js", false, false, false},
            url_test_case{"YouTube", "https://www.youtube.com/watch?v=dQw4w9WgXcQ", false, false, false},
            url_test_case{"YouTube short", "https://youtu.be/dQw4w9WgXcQ", false, false, false},
            url_test_case{"YouTube nocookie", "https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ", false, false, false},
            url_test_case{"YouTube images", "https://i.ytimg.com/vi/dQw4w9WgXcQ/hqdefault.jpg", false, false, false},
            url_test_case{"Google Video", "https://r1---sn-a5mekner.googlevideo.com/videoplayback", false, false, false},
            url_test_case{"Google user content", "https://yt3.googleusercontent.com/ytc/AIdro_m", false, false, false},
            url_test_case{"YouTube Studio", "https://studioyoutube.com/", false, false, false},
            url_test_case{"Chrome Web Store", "https://chromewebstore.google.com/detail/ublock-origin", false, false, false},

            // Third-Party Domains (Forbidden by default)
            url_test_case{"Cloudflare challenge", "https://challenges.cloudflare.com/cdn-cgi/challenge-platform/h/g/turnstile/if/ov2/0/234", true, true, false},
            url_test_case{"Google", "https://www.google.com/", true, true, false},
            url_test_case{"Github", "https://github.com/SteamClientHomebrew/Millennium", true, true, false}
        );

        INFO("Testing URL: " << test_case.url << " (" << test_case.description << ")");
        target_url target(test_case.url);
        std::vector<network_hook_ctl::hook_item> empty_hooks;

        REQUIRE(target.is_safe_for_network_hooks() == test_case.expected_network_safe);
        REQUIRE(target.is_safe_for_js() == test_case.expected_js_safe);
        REQUIRE(target.allows_webkit_injection(empty_hooks) == test_case.expected_webkit_injection);
    }

    SECTION("Third-Party Domains (Allowed if explicitly hooked)") {
        std::vector<network_hook_ctl::hook_item> active_hooks;
        network_hook_ctl::hook_item hook;
        hook.hook.url_pattern = std::regex(".*discord\\.com.*");
        active_hooks.push_back(hook);

        target_url valid_hook("https://www.discord.com/app");
        target_url invalid_hook("https://www.google.com/");

        REQUIRE(valid_hook.allows_webkit_injection(active_hooks));
        REQUIRE_FALSE(invalid_hook.allows_webkit_injection(active_hooks));
    }

    SECTION("Steam ownership vs injection") {
        target_url loopback("https://steamloopback.host/index.html");
        std::vector<network_hook_ctl::hook_item> empty_hooks;
        REQUIRE(loopback.is_steam_owned());
        REQUIRE_FALSE(loopback.allows_webkit_injection(empty_hooks));
    }
}
