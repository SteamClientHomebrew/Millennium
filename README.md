
![Logo](https://cdn.discordapp.com/attachments/923017628367335428/1057455729067429918/icons8-planet-50.png)

## Millennium Steam Patcher for Windows

Join the discord server for updates or for support if something doesn't make sense.

Please don't waste my time if the answer to your question is in this README

https://discord.gg/MXMWEQKgJF
## How to install

run the `installer.exe` in the release folder, and then run steam normally

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


## Compilation 

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
## For Skin Developers

Every skin should contain a ``config.json`` file in the root directory of the steam skin

**Example:**
```
C:\Program Files (x86)\Steam\steamui\skins\skin_name\config.json
```

 - Each config file should include a json key named `patch` which will contain the pages/url in steam for millennium to overwrite.

 - Each page should contain the key `remote` which tells millennium whether or not the page is a remote source, i.e. `store.steampowered.com`

 - Each page should contain a css file, that is in the root directory of the skin or somewhere directly proportional 

i.e. if this is the location of your css file
```
C:\Program Files (x86)\Steam\steamui\skins\skin_name\css_files\styles.css
```
then this is what the `"css"` json key should be 

```json
{
    "css": "css_files/styles.css"
}
```

**IMPORTANT NOTE**
the steam store page and the community tab are collectively one CEF instance, so ther is no need to insert css on both pages together

 - Every page ***CAN*** contain a js file that can be directly injected into the page/window. these files can be blocked from loading if requested from the user.

**NOTE** 

unsafe scripting HAS been enabled meaning Content Security Policy is no longer enabled on remote steam pages, so you can script from cross-origin if you'd like.

**Example config.json**

```json
{
  "patch": [
    {
      "url": "store.steampowered.com",
      "css": "webkit.css",
      "js": "libraryroot.custom.js"
    },
    {
      "url": "Steam",
      "css": "libraryroot.custom.css",
      "js": "steam.js",
      "remote": false
    },
    {
      "url": "Friends List",
      "css": "libraryroot.custom.css",
      "remote": false
    }
    ...
  ]
}
```

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

## TODO

Settins menu is not loaded on any other language other than English

## TODO

~use 100% nlohmann::json instead of rapidJson because its slower and its buggy~

## Authors

- [@ShadowMonster99](https://github.com/ShadowMonster99)


## Helpers

- [@PhantomGamers](https://github.com/PhantomGamers) - discovered browser instance from remote debugger instead of connecting to individual cef instances `json/version`

## Testers

- [@AikoMidori AKA Shiina](https://github.com/AikoMidori)
