<div align="center">
<!-- <img src="https://i.imgur.com/9qYPFSA.png" alt="Alt text" width="40">
  ## Millennium for Steam® -->

<h3><img align="center" height="40" src="https://i.imgur.com/9qYPFSA.png"> &nbsp; &nbsp;Millennium for Steam®</h3>
<br>

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

[loc-bage]: https://img.shields.io/endpoint?url=https%3A%2F%2Floc-counter.onrender.com%2F%3Frepo%3DSteamClientHomebrew%2FMillennium%26branch%3Dmain%26ignored%3Dvendor%26languages%3DC%2520Header%2CC%252B%252B&color=111111&style=for-the-badge&logoColor=white&label=Lines%20of%20Code

Millennium is an **open-source low-code modding framework** to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead.

If you enjoy this tool, please consider starring the project ⭐

<br>

<!-- credits to https://github.com/clawdius for this intro video -->
https://github.com/SteamClientHomebrew/Millennium/assets/81448108/0c4a0ea0-7995-442a-b569-68ca3750fd5e

<br>
</div>

## Installation

### Automatic Installation (Recommended)

  Install Millennium for Windows. See [this page](https://github.com/SteamClientHomebrew/Millennium/wiki/Getting-Started#automatic) for a more detailed guide.

  <!-- [![Windows Installer][windows-badge]][windows-link] -->
[![Static Badge](https://img.shields.io/badge/Download%20Windows-fff?style=for-the-badge&logo=windows&logoColor=white&color=2D5CBF)][windows-link]
<!-- [![Static Badge](https://img.shields.io/badge/Download%20Linux-fff?style=for-the-badge&logo=linux&logoColor=white&color=orange)][windows-link] -->


  [windows-link]: https://github.com/SteamClientHomebrew/Installer/releases/latest/download/Millennium.Installer-Windows.exe
  [windows-badge]: https://img.shields.io/badge/Windows%20(10+)-3a71c1?logo=Windows&logoColor=white&labelColor=111111&color=3a71c1&style=for-the-badge

### Manual Installation

For normal users, installing via the installers makes the most sense. However when wanting to either develop Millennium, or when the installers do not work, this option can be used. Check our [wiki](https://github.com/SteamClientHomebrew/Millennium/wiki/Getting-Started#manual) for a guide on how to do this.

&nbsp;

## Core Features

- ### [Plugin Loader](/src/)
  - **TypeScript ([React](https://react.dev/)) frontend** container in Steam
  - **Python backend** container in [usermode](https://en.wikipedia.org/wiki/User-Mode_Driver_Framework)
  - **[Foreign function interface](https://en.wikipedia.org/wiki/Foreign_function_interface)** binding from **Python** to **JavaScript** and vice versa
  - **Hook modules in the Steam web browser**
    - Overwrite/Modify HTTP requests
    - Load custom JavaScript (Native) into web browser
    - Load custom StyleSheets into web browser
- ### [Core Modules](/assets/)
  - Manage and load custom user themes into Steam
  - Manage custom plugins for steam
  - Maintain theme & plugin versions. 
  - Manage [embedded Python](https://www.python.org/downloads/release/python-3118/) installation
    - Manage [Millennium Python Developer Tools](https://pypi.org/project/millennium/)
    - Custom package manager for all plugins

&nbsp;

## Platform Support

Millennium currently only supports standard Windows NT builds. This includes ARM based processors and x86 based processors. Linux and Mac are NOT currently supported BUT support is currently in development for linux. For a more details see our [wiki](https://github.com/SteamClientHomebrew/Millennium/wiki/Platform-Support)

Supported Platforms:

- Windows 10 and newer
- Windows (x86/x64/ARM) NT
- Linux (WIP, [Learn more](https://github.com/SteamClientHomebrew/Millennium/wiki/Platform-Support))
