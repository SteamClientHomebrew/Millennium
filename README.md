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
[discord-link]: https://steambrew.app/discord

[website-badge]: https://img.shields.io/badge/website-green?labelColor=grey&color=111111&style=for-the-badge&logo=firefoxbrowser&logoColor=white
[website-link]: https://steambrew.app/

[docs-badge]: https://img.shields.io/badge/documentation-green?labelColor=grey&color=111111&style=for-the-badge&logo=readthedocs&logoColor=white
[docs-link]: https://docs.steambrew.app/

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

  Installing Millennium is only a few steps. See [this page](https://docs.steambrew.app/users/installing#automatic) for a more detailed guide.

### Manual Installation

For normal users, installing via the installers makes the most sense. However when wanting to either develop Millennium, or when the installers do not work, this option can be used. Check our [documentation](https://docs.steambrew.app/users/installing#manual) for a guide on how to do this.

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


Supported Platforms:

- Windows (x86/x64/ARM) NT (10 and newer)
- Linux (WIP, [Learn more](https://github.com/SteamClientHomebrew/Millennium/wiki/Platform-Support))

As of now, Millennium only has official support for Windows. Although Millennium works on Linux, its still in development and you should expect bugs. If you face issues on Linux, report them on this repository.