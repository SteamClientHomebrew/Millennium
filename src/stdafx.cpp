#include "stdafx.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <window/core/window.hpp>
#include <GLFW/glfw3native.h>

#ifdef _WIN32
#include <Windows.h> // include windows library on windows
#include <windowsx.h>
#elif __linux__
#include <gtk/gtk.h>
#endif

/**
 * The current version of Millennium. Responsible for deciding when to update 
 */
const char* m_ver = "1.1.4";

void OpenURL(std::string url) {
#ifdef _WIN32
    ShellExecute(0, "open", url.c_str(), 0, 0, SW_SHOWNORMAL);
#elif __linux__
    int _ = system(fmt::format("xdg-open {} &", url.c_str() ).c_str());
#endif
}

#ifdef __linux__
namespace
{
    GtkMessageType getMessageType(Style style)
    {
        switch (style)
        {
            case Style::Info:
                return GTK_MESSAGE_INFO;
            case Style::Warning:
                return GTK_MESSAGE_WARNING;
            case Style::Error:
                return GTK_MESSAGE_ERROR;
            case Style::Question:
                return GTK_MESSAGE_QUESTION;
            default:
                return GTK_MESSAGE_INFO;
        }
    }

    GtkButtonsType getButtonsType(Buttons buttons)
    {
        switch (buttons)
        {
            case Buttons::OK:
                return GTK_BUTTONS_OK;
            case Buttons::OKCancel:
                return GTK_BUTTONS_OK_CANCEL;
            case Buttons::YesNo:
                return GTK_BUTTONS_YES_NO;
            case Buttons::Quit:
                return GTK_BUTTONS_CLOSE;
            default:
                return GTK_BUTTONS_OK;
        }
    }

    Selection getSelection(gint response)
    {
        switch (response)
        {
            case GTK_RESPONSE_OK:
                return Selection::OK;
            case GTK_RESPONSE_CANCEL:
                return Selection::Cancel;
            case GTK_RESPONSE_YES:
                return Selection::Yes;
            case GTK_RESPONSE_NO:
                return Selection::No;
            case GTK_RESPONSE_CLOSE:
                return Selection::Quit;
            default:
                return Selection::None;
        }
    }
} // namespace
namespace msg {
    Selection show(const char* message, const char* title, Style style, Buttons buttons)
    {
        if (!gtk_init_check(0, nullptr))
        {
            return Selection::Error;
        }

        // Create a parent window to stop gtk_dialog_run from complaining
        GtkWidget* parent = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                                   GTK_DIALOG_MODAL,
                                                   getMessageType(style),
                                                   getButtonsType(buttons),
                                                   "%s",
                                                   message);
        gtk_window_set_title(GTK_WINDOW(dialog), title);
        Selection selection = getSelection(gtk_dialog_run(GTK_DIALOG(dialog)));

        gtk_widget_destroy(GTK_WIDGET(dialog));
        gtk_widget_destroy(GTK_WIDGET(parent));
        while (g_main_context_iteration(nullptr, false));

        return selection;
    }
}
#elif _WIN32

namespace
{
#if defined(UNICODE)
    bool utf8ToUtf16(const char* utf8String, std::wstring& utf16String)
    {
        int count = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, nullptr, 0);
        if (count <= 0)
        {
            return false;
        }

        utf16String = std::wstring(static_cast<size_t>(count), L'\0');
        return MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, &utf16String[0], count) > 0;
    }
#endif // defined(UNICODE)

    UINT getIcon(Style style)
    {
        switch (style)
        {
        case Style::Info:
            return MB_ICONINFORMATION;
        case Style::Warning:
            return MB_ICONWARNING;
        case Style::Error:
            return MB_ICONERROR;
        case Style::Question:
            return MB_ICONQUESTION;
        default:
            return MB_ICONINFORMATION;
        }
    }

    UINT getButtons(Buttons buttons)
    {
        switch (buttons)
        {
        case Buttons::OK:
        case Buttons::Quit: // There is no 'Quit' button on Windows :(
            return MB_OK;
        case Buttons::OKCancel:
            return MB_OKCANCEL;
        case Buttons::YesNo:
            return MB_YESNO;
        default:
            return MB_OK;
        }
    }

    Selection getSelection(int response, Buttons buttons)
    {
        switch (response)
        {
        case IDOK:
            return buttons == Buttons::Quit ? Selection::Quit : Selection::OK;
        case IDCANCEL:
            return Selection::Cancel;
        case IDYES:
            return Selection::Yes;
        case IDNO:
            return Selection::No;
        default:
            return Selection::None;
        }
    }
} // namespace

namespace msg
{
    Selection show(const char* message, const char* title, Style style, Buttons buttons)
    {
        UINT flags = MB_TASKMODAL;

        flags |= getIcon(style);
        flags |= getButtons(buttons);

#if defined(UNICODE)
        std::wstring wideMessage;
        std::wstring wideTitle;
        if (!utf8ToUtf16(message, wideMessage) || !utf8ToUtf16(title, wideTitle))
        {
            return Selection::Error;
        }

        const WCHAR* messageArg = wideMessage.c_str();
        const WCHAR* titleArg = wideTitle.c_str();
#else
        const char* messageArg = message;
        const char* titleArg = title;
#endif

        return getSelection(MessageBox(nullptr, messageArg, titleArg, flags), buttons);
    }
}
 // namespace boxer
#endif