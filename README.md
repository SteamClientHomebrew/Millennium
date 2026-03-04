<img width="3844" height="793" alt="image" src="https://github.com/user-attachments/assets/2c662772-bca1-4de4-988f-5304d7dfd87d" />

<div align="center">
<br/>
  <a href="https://steambrew.app/discord">
      <img alt="Static Badge" src="https://img.shields.io/badge/discord-green?labelColor=404040&color=353535&style=for-the-badge&logo=discord&logoColor=white" href="#">
  </a>
  <a href="https://steambrew.app">
      <img alt="Static Badge" src="https://img.shields.io/badge/website-green?labelColor=404040&color=353535&style=for-the-badge&logo=firefoxbrowser&logoColor=white" href="#">
  </a>
  <a href="https://docs.steambrew.app">
      <img alt="Static Badge" src="https://img.shields.io/badge/documentation-green?labelColor=404040&color=353535&style=for-the-badge&logo=readthedocs&logoColor=white" href="#">
  </a>
<br>
<br>
</div>

Millennium is an **open-source low-code modding framework** to create, manage and use themes/plugins<br/> for the desktop Steam Client without any low-level internal interaction or overhead.

If you enjoy this tool, please consider starring the project ⭐

## Installation

Installing Millennium is only a few steps. See [this page](https://docs.steambrew.app/users/installing) for a more detailed guide.

### macOS (Experimental)

macOS support defaults to a passive tier0 install path for direct `Steam.app`
launches. For active development, use `./scripts/launch_macos.sh`.

#### Install

1. Build the macOS runtime and wrapper:

```bash
cmake --preset osx-debug
cmake --build osx-debug
```

2. Install JS dependencies and build bootstrap web assets:

```bash
cd src/typescript
bun install
bun run build
```

3. Install Millennium into Steam's runtime bundle (passive tier0 mode):

```bash
./scripts/install_macos.sh
```

By default, `install_macos.sh` writes `BootStrapperInhibitAll=enable` into
`Steam.cfg`. Use `--skip-steam-cfg-inhibit-all` to skip that for the current run.

4. Start Steam normally from `Steam.app`.

Once Millennium is installed, you can find its related settings within the Steam user interface.

From the Steam menu in the macOS menu bar (next to the Apple logo), select Millennium.

You can also open Millennium from the steam url:

```bash
open 'steam://millennium/settings'
```

You can use `./scripts/install_macos.sh --help` for additional options (`--status`, `--repair`, `--uninstall`, `--restore-steam-cfg`).

#### Run (Development/Debug)

For local development/debug iteration, use the wrapper launcher:

```bash
./scripts/launch_macos.sh
```

To enable Steam DevTools shortcuts on macOS via wrapper launch:

```bash
./scripts/launch_macos.sh --dev --devtools-port 8080
```

Then open DevTools with `F12` or `Ctrl+Shift+I`, or connect Chrome to:

```text
http://127.0.0.1:8080
```

If you need startup trace logs, launch with:

```bash
MILLENNIUM_BOOTSTRAP_TRACE_PATH=/tmp/millennium-bootstrap.trace \
./scripts/launch_macos.sh
```

## Features
### Plugins

-   **TypeScript ([React](https://react.dev/)) frontend** container in Steam
-   **Lua backend** container in [usermode](https://en.wikipedia.org/wiki/User-Mode_Driver_Framework)
-   **[Foreign function interface](https://en.wikipedia.org/wiki/Foreign_function_interface)** binding from **Lua** to **TypeScript** and vice versa
-   **Hook modules in the Steam web browser**
  -   Overwrite/Modify HTTP requests
  -   Load custom JavaScript (Native) into web browser
  -   Load custom StyleSheets into web browser
    
### Themes
-   Manage and load custom themes written in JavaScript and CSS.
-   Inject JavaScript modules and CSS modules into specific Steam windows,
-   Provide customizable color, style, and javascript options for your theme,
    letting users personalize their experience without touching any code

## Official Plugin Repository

Millennium is designed to be fully extensible through plugins.
To ensure version compatibility and safety, we maintain a separate, curated [PluginDatabase](https://github.com/SteamClientHomebrew/PluginDatabase) repository.

All plugins in the PluginDatabase are versioned and reviewed to work seamlessly with the current Millennium release, reducing the risk of conflicts or instability when adding new features.

**For more related information, checkout the [plugin database](https://github.com/SteamClientHomebrew/PluginDatabase)**.

## Adding Languages

Only languages officially supported by the Steam can be added to the Millennium. Check the list [here](https://partner.steamgames.com/doc/store/localization/languages) in the `Full Platform Supported Languages` section.

To add your spoken language to Millennium, fork this repository and place your language json (based on the [english locale](./src/locales/english.json)) among the [current locales](./src/locales). Name the file `{your_language}.json` where `your_language` is the `API language code` from the [supported language list](https://partner.steamgames.com/doc/store/localization/languages) and append the target file to the `localizationFiles` in [this file](./src/typescript/frontend/utils/localization-manager.ts).

Millennium is only maintaining the english language, and if any changes are made to the english locales that don't reflect on a target language, contributors are responsible for updating them.

## Creating Plugins & Themes

Creating themes and plugins for Millennium is relatively straight foward. Our [documentation](https://docs.steambrew.app/developers) goes over the basics of both,
and we have examples for both [plugins](https://github.com/SteamClientHomebrew/PluginTemplate) and [themes](https://github.com/SteamClientHomebrew/ThemeTemplate)

## Platform Support

Supported Platforms:

-   Windows (x86/x64/ARM) NT (10 and newer)
-   Linux (x86/x86_64/i686/i386)
-   macOS (experimental passive tier0 install via `./scripts/install_macos.sh`, development launch via `./scripts/launch_macos.sh`)

## Sponsors

| <img width="25" height="25" alt="image" src="https://github.com/user-attachments/assets/45ad9409-f9dd-4ff4-9737-03386f01a9b2" /> | Free code signing on Windows provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/) |
| :--- | :--- |
