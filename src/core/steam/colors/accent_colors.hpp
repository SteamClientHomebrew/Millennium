#pragma once
#include <string>
#include <iostream>
#include "winrt/Windows.UI.ViewManagement.h"

using namespace winrt;
using namespace Windows::UI::ViewManagement;

std::string hex(UIColorType code) {

    const auto clampRGB = [&](int r, int g, int b, int a) {

        r = max(0, min(255, r));
        g = max(0, min(255, g));
        b = max(0, min(255, b));
        a = max(0, min(255, a));

        std::stringstream ss;
        ss << "#" << std::hex << std::setw(2) << std::setfill('0') << r
            << std::hex << std::setw(2) << std::setfill('0') << g
            << std::hex << std::setw(2) << std::setfill('0') << b
            << std::hex << std::setw(2) << std::setfill('0') << a;
        return ss.str();
        };

    static UISettings const ui_settings{};
    auto accent_color = ui_settings.GetColorValue(code);
    return clampRGB(accent_color.R, accent_color.G, accent_color.B, accent_color.A);
}

std::string rgb(UIColorType code) {
    static UISettings const ui_settings{};
    auto col = ui_settings.GetColorValue(code);

    return std::format("{}, {}, {}", col.R, col.G, col.B);
}

std::string getColorStr() {

    return std::format(
        ":root {{ "
            "--SystemAccentColor: {}; " 
            "--SystemAccentColorLight1: {}; "
            "--SystemAccentColorLight2: {}; "
            "--SystemAccentColorLight3: {}; "
            "--SystemAccentColorDark1: {}; "
            "--SystemAccentColorDark2: {}; "
            "--SystemAccentColorDark3: {}; "

            "--SystemAccentColor-RGB: {}; "
            "--SystemAccentColorLight1-RGB: {}; "
            "--SystemAccentColorLight2-RGB: {}; "
            "--SystemAccentColorLight3-RGB: {}; "
            "--SystemAccentColorDark1-RGB: {}; "
            "--SystemAccentColorDark2-RGB: {}; "
            "--SystemAccentColorDark3-RGB: {}; "
        "}}",
        hex(UIColorType::Accent),
        hex(UIColorType::AccentLight1),
        hex(UIColorType::AccentLight2),
        hex(UIColorType::AccentLight3),
        hex(UIColorType::AccentDark1),
        hex(UIColorType::AccentDark2),
        hex(UIColorType::AccentDark3),

        rgb(UIColorType::Accent),
        rgb(UIColorType::AccentLight1),
        rgb(UIColorType::AccentLight2),
        rgb(UIColorType::AccentLight3),
        rgb(UIColorType::AccentDark1),
        rgb(UIColorType::AccentDark2),
        rgb(UIColorType::AccentDark3));
}