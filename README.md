
![Logo](https://cdn.discordapp.com/attachments/923017628367335428/1057455729067429918/icons8-planet-50.png)

## Millennium Steam Patcher for Windows

- [Millennium Steam Patcher for Windows](#millennium-steam-patcher-for-windows)
- [What is Millennium](#what-is-millennium)
- [Can I Get Banned?](#can-i-get-banned-)
- [How to install](#how-to-install)
  * [Compiling for yourself?](#compiling-for-yourself-)
- [Features](#features)
    + [Custom CSS Insertion](#custom-css-insertion)
    + [Custom JS Insertion (Enabled by default)](#custom-js-insertion--enabled-by-default-)
- [For Skin Developers](#for-skin-developers)
  * [Have your own installer?](#have-your-own-installer-)
- [Matching against pages with variable titles](#matching-against-pages-with-variable-titles)
    + [configuration settings have been inherited from SFP and should be completely compatible in the important areas](#configuration-settings-have-been-inherited-from-sfp-and-should-be-completely-compatible-in-the-important-areas)
  * [How to find steam window title pages](#how-to-find-steam-window-title-pages)
- [Authors](#authors)

<small><i><a href='http://ecotrust-canada.github.io/markdown-toc/'>Table of contents generated with markdown-toc</a></i></small>




Join the discord server for updates or for support if something doesn't make sense.

Please don't waste my time if the answer to your question is in this README

https://discord.gg/MXMWEQKgJF

## What is Millennium

Millennium is a steam patch that allows skinning on the new steam client. Millennium is a patcher that does not run externally to steam and does NOT require any apps to be running while you have a skin active. Millennium is a headless dll that loads along side steam. Millennium is not injected into the steam application as it is loaded willingly by the steam application. all code runs under steams PPID and requires no attention to maintain its functionality.

## Can I Get Banned?

No you will not. It is possible that can get your steam account banned if you advertise skinning as its no longer a supported thing. However, steam does not actively check if you are skinning and therefor CANNOT detect if you are. You also will NOT get banned on any games when you run steam with the patcher as it doesnt do anything malicious, code isnt packed, doesnt read process memory from protected regions of the steam client, and doesnt ever directly interact with the steam client.

## How to install

run the `installer.exe` in the release folder, and then run steam normally. It will prompt you to restart, then restart steam go to settings and interface, then select a skin to use.


### Compiling for yourself? 

Compile millennium with platform toolset v143 or v142, use ``/std:c++20``

Required [vcpkg](https://vcpkg.io/)'s
```bash
  vcpkg install boost:x86-windows
  vcpkg install cpp-netlib:x86-windows
  vcpkg install rapidjson:x86-windows
  vcpkg install nlohmann-json:x86-windows
  vcpkg install websocketpp:x86-windows
```
Do not use static lib-crypto during compilation, use dynamic linking when if available. 


## Features

**Millennium does not require any other apps open other than the Steam Client**

#### Custom CSS Insertion
- Add custom CSS to all pages/windows in the new steam client
- you can manage what files and what windows do what
- Supports webkit insertions
- Supports steam big picture mode

#### Custom JS Insertion (Enabled by default)
- Can be disabled at request if you believe it may be malicious
- Add custom JS to all pages/windows in the new steam client
- you can manage what files and what windows do what
- Supports webkit insertions
- Supports steam big picture mode

**IF THE SKIN YOU ARE USING HAS OBFUSCATED JS CODE, YOU DON'T UNDERSTAND IT, OR YOU DON'T TRUST THE DEVELOPER, DO NOT ALLOW JS INSERTION**

How to turn it off

```
Steam -> Steam Settings -> Millennium -> Allow Javascript -> false (restart steam)
```

## For Skin Developers

Every skin should contain a `skin.json` file in the root directory of the steam skin

**Example:**
```
C:\Program Files (x86)\Steam\steamui\skins\skin_name\skin.json
```

 - Each config file should include a json key named `Patches` which will contain the pages/url in steam for millennium to overwrite.

 - Each page should contain a css file, that is in the root directory of the skin or somewhere directly proportional 

i.e. if this is the location of your css file
```
C:\Program Files (x86)\Steam\steamui\skins\skin_name\css_files\styles.css
```
then this is what the `"TargetCss"` json key should be 

```json
{
    "TargetCss": "css_files/styles.css"
}
```

**IMPORTANT NOTE**
the steam store page and the community tab are collectively one CEF instance, so ther is no need to insert css on both pages together

 - Every page ***CAN*** contain a js file that can be directly injected into the page/window. these files can be blocked from loading if requested from the user.

**NOTE** 

unsafe scripting HAS been enabled meaning Content Security Policy is no longer enabled on remote steam pages, so you can script from cross-origin if you'd like.

**Default Patches in skin.json**

```json
{
  "Patches": [
    {
      "MatchRegexString": ".*http.*steam.*",
      "TargetCss": "webkit.css",
      "TargetJs": "webkit.js"
    },
    {
      "MatchRegexString": "^Steam$",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": "^OverlayBrowser_Browser$",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": "^SP Overlay:",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": "Supernav$",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": "^notificationtoasts_",
      "TargetCss": "notifications.custom.css",
      "TargetJs": "notifications.custom.js"
    },
    {
      "MatchRegexString": "^SteamBrowser_Find$",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": "^OverlayTab\\d+_Find$",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": "^Steam Big Picture Mode$",
      "TargetCss": "bigpicture.custom.css",
      "TargetJs": "bigpicture.custom.js"
    },
    {
      "MatchRegexString": "^QuickAccess_",
      "TargetCss": "bigpicture.custom.css",
      "TargetJs": "bigpicture.custom.js"
    },
    {
      "MatchRegexString": "^MainMenu_",
      "TargetCss": "bigpicture.custom.css",
      "TargetJs": "bigpicture.custom.js"
    },
    {
      "MatchRegexString": ".friendsui-container",
      "TargetCss": "friends.custom.css",
      "TargetJs": "friends.custom.js"
    },
    {
      "MatchRegexString": "Menu$",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": ".ModalDialogPopup",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    },
    {
      "MatchRegexString": ".FullModalOverlay",
      "TargetCss": "libraryroot.custom.css",
      "TargetJs": "libraryroot.custom.js"
    }
  ]
}
```

If you would like to use Millenniums's default config you can simply omit the skin.json file or include this:
```
{
    "UseDefaultPatches": true
}
```

If that key is included along with custom patches, the custom patches will be applied first, followed by the default patches.

### Have your own installer?

Millennium supports third party installers for skins if you wish to have your own installer. 

The silent installer is a headless exe that will run and install Millennium to steam whenever its ran.

you can select whether it install or uninstall millennium as well as programmatically select a skin for the user.

heres a C# example

```cs
//your skin installation code

static void InstallMillennium()
{
    ProcessStartInfo startInfo = new ProcessStartInfo();
    startInfo.FileName = "millennium.exe";
    startInfo.RedirectStandardOutput = true;
    startInfo.UseShellExecute = false;
    startInfo.Arguments = "install=true or false skin=\"fluent for steam\"";

    Process process = new Process();
    process.StartInfo = startInfo;
    process.OutputDataReceived += Process_OutputDataReceived;
    process.Start();

    process.BeginOutputReadLine();
    process.WaitForExit();
    process.Close();
}

static void Process_OutputDataReceived(object sender, DataReceivedEventArgs e)
{
    if (!string.IsNullOrEmpty(e.Data))
    {
        Console.WriteLine(e.Data);
    }
}
```

if you dont want to use the installer you can parse release info from here
https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest

and setting a skin is done through the registry
`Computer\HKEY_CURRENT_USER\Software\Millennium` and the HiveKey `active-skin` as a REG_SZ (const char*/string)

DO NOT EDIT HiveKey `allow-javascript` without asking the user before hand. otherwise i will blacklist your installer or warn users your installer is malicious

## Matching against pages with variable titles
Certain pages will have titles that change either depending on the user's language settings or some other factor.

In order to match against these pages, you can match against a selector that exists within the page. Millennium will try to match against a query selector in the MatchRegexString.

For example:

- The Friends List and Chat windows can be matched against with `.friendsui-container`

- The library game properties dialog and most of the dialogs that pop up in the overlay menu can be matched against with `.ModalDialogPopup`

- The sign in page can be matched against with `.FullModalOverlay`

#### configuration settings have been inherited from SFP and should be completely compatible in the important areas

### How to find steam window title pages

To find the steam title pages you can either use the title of the window thats visible from the taskbar, or you can go to 

```url
http://localhost:8080/
```
**ONLY AVAILABLE WHEN REMOTE DEBUGGING IS ENABLED WITH**
```
-cef-enable-debugging
```

where you can find all the steam pages, there name, and customize css on pages you may not be able to with `-dev` mode.

if you think this documentation needs more information, join the discord server and ask for a better explaination on something.

## Authors

- [@ShadowMonster99](https://github.com/ShadowMonster99)
