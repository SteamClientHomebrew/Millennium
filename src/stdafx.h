#pragma once
#include <core/steam/application.hpp>
#include <nlohmann/json.hpp>
#include <utils/cout/logger.hpp>
#include <utils/http/http_client.hpp>
#include <fmt/core.h>
#include <imgui.h>


void OpenURL(std::string url);

enum class Style
{
	Info,
	Warning,
	Error,
	Question
};

/*!
 * Options for buttons to provide on a message box
 */
enum class Buttons
{
	OK,
	OKCancel,
	YesNo,
	Quit
};

/*!
 * Possible responses from a message box. 'None' signifies that no option was chosen, and 'Error' signifies that an
 * error was encountered while creating the message box.
 */
enum class Selection
{
	OK,
	Cancel,
	Yes,
	No,
	Quit,
	None,
	Error
};

namespace msg {

	/*!
	 * The default style to apply to a message box
	 */
	constexpr Style kDefaultStyle = Style::Info;

	/*!
	 * The default buttons to provide on a message box
	 */
	constexpr Buttons kDefaultButtons = Buttons::OK;

	/*!
	 * Blocking call to create a modal message box with the given message, title, style, and buttons
	 */
	Selection show(const char* message, const char* title, Style style, Buttons buttons);

	/*!
	 * Convenience function to call show() with the default buttons
	 */
	inline Selection show(const char* message, const char* title, Style style)
	{
		return show(message, title, style, kDefaultButtons);
	}

	/*!
	 * Convenience function to call show() with the default style
	 */
	inline Selection show(const char* message, const char* title, Buttons buttons)
	{
		return show(message, title, kDefaultStyle, buttons);
	}

	/*!
	 * Convenience function to call show() with the default style and buttons
	 */
	inline Selection show(const char* message, const char* title)
	{
		return show(message, title, kDefaultStyle, kDefaultButtons);
	}
}

extern const char* m_ver;
static const char* repo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";