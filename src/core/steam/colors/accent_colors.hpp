#pragma once
#include <string>
#include <iostream>
#ifdef _WIN32
#include <minmax.h>
#elif __linux__
#include <window/core/colors.hpp>
#endif

#ifdef _WIN32 
#include "winrt/Windows.UI.ViewManagement.h"
using namespace winrt;
using namespace Windows::UI::ViewManagement;

/**
 * @brief Converts a UIColorType code to its hexadecimal representation.
 *
 * This function retrieves the color value corresponding to the specified UIColorType code
 * from the UI settings. It then converts the color components (RGB + alpha) to their
 * hexadecimal representation and returns the concatenated hexadecimal string.
 *
 * @param code The UIColorType code representing the color.
 * @return The hexadecimal representation of the color.
 */
std::string hex(UIColorType code) {

    // Lambda function to clamp RGB values and convert them to hexadecimal.
    const auto clampRGB = [&](int r, int g, int b, int a) {

        // Clamp RGB values to range [0, 255].
        r = max(0, min(255, r));
        g = max(0, min(255, g));
        b = max(0, min(255, b));
        a = max(0, min(255, a));

        // Convert RGB values to hexadecimal and concatenate them.
        std::stringstream ss;
        ss << "#" << std::hex << std::setw(2) << std::setfill('0') << r
            << std::hex << std::setw(2) << std::setfill('0') << g
            << std::hex << std::setw(2) << std::setfill('0') << b
            << std::hex << std::setw(2) << std::setfill('0') << a;
        return ss.str();
    };

    // Get the color value from UI settings.
    static UISettings const ui_settings{};
    auto accent_color = ui_settings.GetColorValue(code);

    // Convert color components to hexadecimal and return the result.
    return clampRGB(accent_color.R, accent_color.G, accent_color.B, accent_color.A);
}

/**
 * @brief Converts a UIColorType code to its RGB representation.
 *
 * This function retrieves the color value corresponding to the specified UIColorType code
 * from the UI settings. It then returns the RGB components of the color as a string in
 * the format "R, G, B".
 *
 * @param code The UIColorType code representing the color.
 * @return The RGB representation of the color.
 */
std::string rgb(UIColorType code) {
    // Get the color value from UI settings.
    static UISettings const ui_settings{};
    auto col = ui_settings.GetColorValue(code);

    // Return the RGB components as a string.
    return fmt::format("{}, {}, {}", col.R, col.G, col.B);
}

/**
 * @brief Generates a CSS color string with system accent colors.
 *
 * This function constructs a CSS color string with various system accent colors
 * and their RGB representations, using the hex() and rgb() functions.
 *
 * @return The generated CSS color string.
 */
std::string getColorStr() {

    return 
    (":root { "
        "--SystemAccentColor: " + hex(UIColorType::Accent) + "; "
        "--SystemAccentColorLight1: " + hex(UIColorType::AccentLight1) + "; "
        "--SystemAccentColorLight2: " + hex(UIColorType::AccentLight2) + "; "
        "--SystemAccentColorLight3: " + hex(UIColorType::AccentLight3) + "; "
        "--SystemAccentColorDark1: " + hex(UIColorType::AccentDark1) + "; "
        "--SystemAccentColorDark2: " + hex(UIColorType::AccentDark2) + "; "
        "--SystemAccentColorDark3: " + hex(UIColorType::AccentDark3) + "; "

        "--SystemAccentColor-RGB: " + rgb(UIColorType::Accent) + "; "
        "--SystemAccentColorLight1-RGB: " + rgb(UIColorType::AccentLight1) + "; "
        "--SystemAccentColorLight2-RGB: " + rgb(UIColorType::AccentLight2) + "; "
        "--SystemAccentColorLight3-RGB: " + rgb(UIColorType::AccentLight3) + "; "
        "--SystemAccentColorDark1-RGB: " + rgb(UIColorType::AccentDark1) + "; "
        "--SystemAccentColorDark2-RGB: " + rgb(UIColorType::AccentDark2) + "; "
        "--SystemAccentColorDark3-RGB: " + rgb(UIColorType::AccentDark3) + "; "
    "}");
}
#elif __linux__

struct RGB {
    unsigned int r, g, b;
};

RGB hex_to_rgb(std::string& hexColor)
{
    if (hexColor[0] == '#') {
        hexColor.erase(0, 1);
    }

    std::stringstream ss;
    ss << std::hex << hexColor;
    unsigned int hexValue;
    ss >> hexValue;

    RGB rgbColor = {
        (hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF
    };
    return rgbColor;
}

std::string rgb_to_hex(unsigned int red, unsigned int green, unsigned int blue) {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << std::hex << red
       << std::setw(2) << std::hex << green << std::setw(2) << std::hex << blue;
    return "#" + ss.str();
}

std::string adjust_bright(std::string hexColor, int percentChange) {

    RGB rgb = hex_to_rgb(hexColor);

    double factor = (100.0 + percentChange) / 100.0;
    rgb.r = std::min(255, static_cast<int>(rgb.r * factor));
    rgb.g = std::min(255, static_cast<int>(rgb.g * factor));
    rgb.b = std::min(255, static_cast<int>(rgb.b * factor));

    return rgb_to_hex(rgb.r, rgb.g, rgb.b);
}

std::string getColorStr() {

    auto col = Settings::Get<std::string>("accent-col");

    std::string colors = (":root { "
        "--SystemAccentColor: " + col + "; "
        "--SystemAccentColorLight1: " + adjust_bright(col, 25) + "; "
        "--SystemAccentColorLight2: " + adjust_bright(col, 50) + "; "
        "--SystemAccentColorLight3: " + adjust_bright(col, 75) + "; "
        "--SystemAccentColorDark1: " + adjust_bright(col, -25) + "; "
        "--SystemAccentColorDark2: " + adjust_bright(col, -50) + "; "
        "--SystemAccentColorDark3: " + adjust_bright(col, -75) + "; "
    "}");
    return colors;
}
#endif