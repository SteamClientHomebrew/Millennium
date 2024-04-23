#include <boxer/boxer.h>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace boxer
{

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

} // namespace boxer
