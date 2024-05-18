
<div align="">

<img src="https://i.imgur.com/9qYPFSA.png" alt="Alt text" width="40">

## Millennium for Steam®

[![GitHub Releases][downloads-badge]][downloads-link] [![Discord][discord-badge]][discord-link] [![Website][website-badge]][website-link] [![Docs][docs-badge]][docs-link]

[downloads-badge]: https://img.shields.io/github/downloads/shadowmonster99/millennium-steam-binaries/total?labelColor=0c0d10&color=3a71c1&style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iNDgiIGhlaWdodD0iNDgiIHZpZXdCb3g9IjAgMCA0OCA0OCIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTEyLjI1IDM4LjVIMzUuNzVDMzYuNzE2NSAzOC41IDM3LjUgMzkuMjgzNSAzNy41IDQwLjI1QzM3LjUgNDEuMTY4MiAzNi43OTI5IDQxLjkyMTIgMzUuODkzNSA0MS45OTQyTDM1Ljc1IDQySDEyLjI1QzExLjI4MzUgNDIgMTAuNSA0MS4yMTY1IDEwLjUgNDAuMjVDMTAuNSAzOS4zMzE4IDExLjIwNzEgMzguNTc4OCAxMi4xMDY1IDM4LjUwNThMMTIuMjUgMzguNUgzNS43NUgxMi4yNVpNMjMuNjA2NSA2LjI1NThMMjMuNzUgNi4yNUMyNC42NjgyIDYuMjUgMjUuNDIxMiA2Ljk1NzExIDI1LjQ5NDIgNy44NTY0N0wyNS41IDhWMjkuMzMzTDMwLjI5MzEgMjQuNTQwN0MzMC45NzY1IDIzLjg1NzMgMzIuMDg0NiAyMy44NTczIDMyLjc2OCAyNC41NDA3QzMzLjQ1MTQgMjUuMjI0MiAzMy40NTE0IDI2LjMzMjIgMzIuNzY4IDI3LjAxNTZMMjQuOTg5OCAzNC43OTM4QzI0LjMwNjQgMzUuNDc3MiAyMy4xOTg0IDM1LjQ3NzIgMjIuNTE1IDM0Ljc5MzhMMTQuNzM2OCAyNy4wMTU2QzE0LjA1MzQgMjYuMzMyMiAxNC4wNTM0IDI1LjIyNDIgMTQuNzM2OCAyNC41NDA3QzE1LjQyMDIgMjMuODU3MyAxNi41MjgyIDIzLjg1NzMgMTcuMjExNyAyNC41NDA3TDIyIDI5LjMyOVY4QzIyIDcuMDgxODMgMjIuNzA3MSA2LjMyODgxIDIzLjYwNjUgNi4yNTU4TDIzLjc1IDYuMjVMMjMuNjA2NSA2LjI1NThaIiBmaWxsPSIjM2E3MWMxIi8+Cjwvc3ZnPgo=
[downloads-link]: #automatic-installation-recommended

[discord-badge]: https://img.shields.io/badge/discord-green?labelColor=0c0d10&color=7289da&style=for-the-badge&logo=discord&logoColor=7289da
[discord-link]: https://millennium.web.app/discord

[website-badge]: https://img.shields.io/badge/website-green?labelColor=0c0d10&color=3a71c1&style=for-the-badge&logo=firefoxbrowser&logoColor=3a71c1
[website-link]: https://millennium.web.app/

[docs-badge]: https://img.shields.io/badge/wiki-green?labelColor=0c0d10&color=3a71c1&style=for-the-badge&logo=readthedocs&logoColor=3a71c1
[docs-link]: https://github.com/SteamClientHomebrew/Millennium/wiki

Millennium is an **open-source low-code modding framework** to create, manage and use themes for the desktop Steam Client without any low-level internal interaction or overhead.

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
  [windows-badge]: https://img.shields.io/badge/Windows%20(10+)-3a71c1?logo=Windows&logoColor=3a71c1&labelColor=0c0d10&color=3a71c1&style=for-the-badge

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
