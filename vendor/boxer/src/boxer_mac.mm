#include <boxer/boxer.h>
#import <Cocoa/Cocoa.h>

namespace boxer
{

namespace
{
#if defined(MAC_OS_X_VERSION_10_12) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
   const NSAlertStyle kInformationalStyle = NSAlertStyleInformational;
   const NSAlertStyle kWarningStyle = NSAlertStyleWarning;
   const NSAlertStyle kCriticalStyle = NSAlertStyleCritical;
#else
   const NSAlertStyle kInformationalStyle = NSInformationalAlertStyle;
   const NSAlertStyle kWarningStyle = NSWarningAlertStyle;
   const NSAlertStyle kCriticalStyle = NSCriticalAlertStyle;
#endif

#if defined(MAC_OS_X_VERSION_10_9) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
   using ModalResponse = NSModalResponse;
#elif defined(MAC_OS_X_VERSION_10_5) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
   using ModalResponse = NSInteger;
#else
   using ModalResponse = int;
#endif

   NSString* const kOkStr = @"OK";
   NSString* const kCancelStr = @"Cancel";
   NSString* const kYesStr = @"Yes";
   NSString* const kNoStr = @"No";
   NSString* const kQuitStr = @"Quit";

   NSAlertStyle getAlertStyle(Style style)
   {
      switch (style)
      {
      case Style::Info:
         return kInformationalStyle;
      case Style::Warning:
         return kWarningStyle;
      case Style::Error:
         return kCriticalStyle;
      case Style::Question:
         return kWarningStyle;
      default:
         return kInformationalStyle;
      }
   }

   void setButtons(NSAlert* alert, Buttons buttons)
   {
      switch (buttons)
      {
      case Buttons::OK:
         [alert addButtonWithTitle:kOkStr];
         break;
      case Buttons::OKCancel:
         [alert addButtonWithTitle:kOkStr];
         [alert addButtonWithTitle:kCancelStr];
         break;
      case Buttons::YesNo:
         [alert addButtonWithTitle:kYesStr];
         [alert addButtonWithTitle:kNoStr];
         break;
     case Buttons::Quit:
         [alert addButtonWithTitle:kQuitStr];
       break;
      default:
         [alert addButtonWithTitle:kOkStr];
      }
   }

   Selection getSelection(ModalResponse index, Buttons buttons)
   {
      switch (buttons)
      {
      case Buttons::OK:
         return index == NSAlertFirstButtonReturn ? Selection::OK : Selection::None;
      case Buttons::OKCancel:
         if (index == NSAlertFirstButtonReturn)
         {
            return Selection::OK;
         }
         else if (index == NSAlertSecondButtonReturn)
         {
            return Selection::Cancel;
         }
         else
         {
            return Selection::None;
         }
      case Buttons::YesNo:
         if (index == NSAlertFirstButtonReturn)
         {
            return Selection::Yes;
         }
         else if (index == NSAlertSecondButtonReturn)
         {
            return Selection::No;
         }
         else
         {
            return Selection::None;
         }
      case Buttons::Quit:
         return index == NSAlertFirstButtonReturn ? Selection::Quit : Selection::None;
      default:
         return Selection::None;
      }
   }
} // namespace

Selection show(const char* message, const char* title, Style style, Buttons buttons)
{
   NSAlert* alert = [[NSAlert alloc] init];

   [alert setMessageText:[NSString stringWithUTF8String:title]];
   [alert setInformativeText:[NSString stringWithUTF8String:message]];

   [alert setAlertStyle:getAlertStyle(style)];
   setButtons(alert, buttons);

   // Force the alert to appear on top of any other windows
   [[alert window] setLevel:NSModalPanelWindowLevel];

   Selection selection = getSelection([alert runModal], buttons);
   [alert release];

   return selection;
}

} // namespace boxer
