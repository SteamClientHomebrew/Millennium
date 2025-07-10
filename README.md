<div align="center">
<!-- <img src="https://i.imgur.com/9qYPFSA.png" alt="Alt text" width="40">
  ## Millennium for Steam® -->

<h3><img align="center" height="40" src="https://i.imgur.com/9qYPFSA.png"> &nbsp; &nbsp;Millennium for Steam®</h3>

<kbd>
  <a href="https://steambrew.app/discord">
      <img alt="Static Badge" src="https://img.shields.io/badge/discord-green?labelColor=151B23&color=151B23&style=for-the-badge&logo=discord&logoColor=white" href="#">
  </a>
</kbd>
<kbd>
  <a href="https://steambrew.app">
      <img alt="Static Badge" src="https://img.shields.io/badge/website-green?labelColor=151B23&color=151B23&style=for-the-badge&logo=firefoxbrowser&logoColor=white" href="#">
  </a>
</kbd>
<kbd>
  <a href="https://docs.steambrew.app">
      <img alt="Static Badge" src="https://img.shields.io/badge/documentation-green?labelColor=151B23&color=151B23&style=for-the-badge&logo=readthedocs&logoColor=white" href="#">
  </a>
</kbd>

<br>
<br>

Millennium is an **open-source low-code modding framework** to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead.

If you enjoy this tool, please consider starring the project ⭐

<br>
</div>

## Installation

### Automatic Installation (Recommended)

Installing Millennium is only a few steps. See [this page](https://docs.steambrew.app/users/installing) for a more detailed guide.

### Manual Installation

For normal users, installing via the installers makes the most sense. However when wanting to either develop Millennium, or when the installers do not work, this option can be used. Check our [documentation](https://docs.steambrew.app/users/installing#manual) for a guide on how to do this.

&nbsp;

## Core Features

-   ### [Plugin Loader](/src/)
    -   **TypeScript ([React](https://react.dev/)) frontend** container in Steam
    -   **Python backend** container in [usermode](https://en.wikipedia.org/wiki/User-Mode_Driver_Framework)
    -   **[Foreign function interface](https://en.wikipedia.org/wiki/Foreign_function_interface)** binding from **Python** to **JavaScript** and vice versa
    -   **Hook modules in the Steam web browser**
        -   Overwrite/Modify HTTP requests
        -   Load custom JavaScript (Native) into web browser
        -   Load custom StyleSheets into web browser
-   ### [Core Modules](/assets/)
    -   Manage and load custom user themes into Steam
    -   Manage custom plugins for steam
    -   Maintain theme & plugin versions.
    -   Manage [embedded Python](https://www.python.org/downloads/release/python-3118/) installation
        -   Manage [Millennium Python Developer Tools](https://pypi.org/project/millennium/)
        -   Custom package manager for all plugins

&nbsp;

## Adding Spoken Language Support

Take a look [here](./assets#adding-languages), it quickly covers what files you'll need to edit in order to add your spoken language to Millennium!

&nbsp;

## Creating Plugins & Themes

Creating themes and plugins for Millennium is relatively straight foward. Our [documentation](https://docs.steambrew.app/developers) goes over the basics of both,
and we have examples for both in [examples](./examples)

&nbsp;

## Platform Support

Supported Platforms:

-   Windows (x86/x64/ARM) NT (10 and newer)
-   Linux (x86/x86_64/i686/i386)
-   OSX (Support planned, WIP)

&nbsp;

## Unix File Structure

```bash
/
├─ usr/
│  ├─ lib/
│  │  ├─ millennium/
│  │  │  ├─ libMillennium.so # millennium
│  │  │  ├─ libpython-3.11.8.so # dynamically linked to millennium, allows user plugin backends to run
│  ├─ share/
│  │  ├─ assets/ # builtin plugin that provides base functionality for millennium.
│  │  ├─ shims/
├─ home/
│  ├─ $user/
│  │  ├─ .local/share
│  │  │  ├─ millennium/
│  │  │  │  ├─ .venv/
│  │  │  │  ├─ logs/
│  │  │  │  ├─ plugins/ # user plugins
│  │  ├─ .config/
│  │  │  ├─ millennium/ # config files
```

&nbsp;

## Building from source

### Windows

1.  Download and install [MSYS2](https://repo.msys2.org/distrib/x86_64/msys2-x86_64-20241208.exe)
1.  Download and install [Visual Studio Build Tools](https://aka.ms/vs/17/release/vs_BuildTools.exe)
1.  Download and install [Visual Studio Code](https://code.visualstudio.com/)
1.  Open the MinGW32 shell and run

    ```cmd
    cd somewhere/you/want/to/put/millennium
    pacman -Syu && pacman -S --needed git mingw-w64-i686-cmake base-devel mingw-w64-i686-toolchain

    git clone https://github.com/SteamClientHomebrew/Millennium --recursive
    cd Millennium
    ./vendor/vcpkg/bootstrap-vcpkg.sh
    ```

1.  Download Millennium's [python backend](https://github.com/SteamClientHomebrew/PythonBuildAgent/releases/tag/v1.0.7) for Windows, and copy the files to `%MILLENNIUM_SRC_DIR%/vendor/python`
1.  Build Millennium

    ```cmd
    code .
    ```

    In Visual Studio Code, make sure you have:

    -   C/C++ Extension Pack
    -   Prettier

    Then use CTRL+SHIFT+B
    This will build Millennium and all of its submodules.
    You can then run Steam with Millennium.

### Linux

> [!NOTE]  
> This guide assumes you're using Visual Studio Code to develop. If you're using an IDE like CLion or Code::Blocks, steps are _probably_
> similar but they aren't documented here yet.
>
> If you're using just a basic code editor like emacs, vim, micro, etc. you'll need to run build commands manually.

1.  Setup

    In Visual Studio Code, make sure you have:

        * C/C++ Extension Pack
        * Prettier

1.  Clone and Setup Repository

    ```bash
    cd somewhere/you/want/to/put/millennium
    git clone https://github.com/SteamClientHomebrew/Millennium --recursive
    cd Millennium
    ./vendor/vcpkg/bootstrap-vcpkg.sh
    ```

1.  Build Millennium

    Use CTRL+SHIFT+B

    This will build Millennium and all of its submodules.
    You can then run Steam with Millennium.
