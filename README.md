<div align="center">
<img src="https://i.imgur.com/9qYPFSA.png" alt="Alt text" width="40">
  
## Millennium for Steam®

[![GitHub Releases][downloads-badge]][downloads-link] 
![Endpoint Badge][loc-bage]
[![Discord][discord-badge]][discord-link] 
[![Website][website-badge]][website-link] [![Docs][docs-badge]][docs-link]


[downloads-badge]: https://img.shields.io/github/downloads/shadowmonster99/millennium-steam-binaries/total?labelColor=grey&color=111111&style=for-the-badge
[downloads-link]: #automatic-installation-recommended

[discord-badge]: https://img.shields.io/badge/discord-green?labelColor=grey&color=111111&style=for-the-badge&logo=discord&logoColor=white
[discord-link]: https://millennium.web.app/discord

[website-badge]: https://img.shields.io/badge/website-green?labelColor=grey&color=111111&style=for-the-badge&logo=firefoxbrowser&logoColor=white
[website-link]: https://millennium.web.app/

[docs-badge]: https://img.shields.io/badge/documentation-green?labelColor=grey&color=111111&style=for-the-badge&logo=readthedocs&logoColor=white
[docs-link]: https://github.com/SteamClientHomebrew/Millennium/wiki

[loc-bage]: https://img.shields.io/endpoint?url=https://ghloc.vercel.app/api/SteamClientHomebrew/Millennium/badge?filter=.cc$,.cpp$,.h$,.hpp$?&color=111111&style=for-the-badge&logoColor=white&label=Lines%20of%20Code

Millennium is an **open-source low-code modding framework** to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead.

If you enjoy this tool, please consider starring the project ⭐

<div align="" style="box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1);">
  <br>
  <div style="box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1);">
    <img alt="Preview" alt="Hero image" src="https://i.imgur.com/cfPZJot.png"/>
  </div>
</div>

<br><br>

</div>

## Installation

### Automatic Installation (Recommended)

  Install Millennium for Windows. See [this page](https://github.com/SteamClientHomebrew/Millennium/wiki/Getting-Started#automatic) for a more detailed guide.

  [![Windows Installer][windows-badge]][windows-link]

  [windows-link]: https://github.com/SteamClientHomebrew/Installer/releases/latest/download/Millennium.Installer-Windows.exe
  [windows-badge]: https://img.shields.io/badge/Windows%20(10+)-3a71c1?logo=Windows&logoColor=white&labelColor=111111&color=3a71c1&style=for-the-badge

### Manual Installation

For normal users, installing via the installers makes the most sense. However when wanting to either develop Millennium, or when the installers do not work, this option can be used. Check our [wiki](https://github.com/SteamClientHomebrew/Millennium/wiki/Getting-Started#manual) for a guide on how to do this.

<br>

## Core  Features

- CSS Overrides
  - Client Area CSS Overrides
  - Webkit CSS Overrides
- JavaScript Overrides
  - high-level interation with React in Client area.
  - Interation with any aspect of the Client with native JS.

## RoadMap

- Linux Support
- Faster patching algorithm for Settings Modal
- Plugin Support
  - Backends written in python
  - Frontend written with React or native JS
  - IPC between usermode and JS vm.

## Platform Support

Millennium currently only supports standard Windows NT builds. This includes ARM based processors and x86 based processors. Linux and Mac are NOT currently supported BUT support is currently in development for linux. For a more details see our [wiki](https://github.com/SteamClientHomebrew/Millennium/wiki/Platform-Support)

Supported Platforms:

- Windows 10 and newer
- Windows (x86/x64/ARM) NT
- Linux (WIP, [Learn more](https://github.com/SteamClientHomebrew/Millennium/wiki/Platform-Support))
