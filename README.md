# Millennium

[Home Page](https://steambrew.app/) • [Discord](https://steambrew.app/discord) • [Documentation](https://docs.steambrew.app/)

Open-source modding framework for creating and managing Steam Client themes and plugins.

`Windows` `Linux` 

![Millennium Settings Page Preview](https://github.com/user-attachments/assets/97478b09-4a98-487d-96de-e5ccb6282a73)

## Installation[^1]

Installing Millennium is only a few steps. See [here](https://docs.steambrew.app/users/installing) for a more detailed guide.

## Building Source

Refer to [BUILDING.md](.github/docs/BUILDING.md) for build instructions for your platform. 

## Features[^2]

* Custom CSS and Themes: Inbuilt css editor with live-edit capability
* Plugin support allowing full control over Steam
* Exceptionally optimized with a very minimal footprint
* Privacy friendly: blocks Steam analytics & crash reporting out of the box and has no in-built telemetry
* Fully portable
* Maintained very actively, most widespread bugs are fixed in hours.
* Yours. Use it how *you* want to.

## Language Support[^3]

### Adding a new locale

Only languages officially supported by the Steam can be added to the Millennium. 
Check the list [here](https://partner.steamgames.com/doc/store/localization/languages) in the **"Full Platform Supported Languages"** section.
Base your locale on the existing [english](./src/locales/english.json) locale, and place it among the [existing](./src/locales) locales. 
Name the file **lang.json** where *lang* is the **"API language code"** from the [supported language list](https://partner.steamgames.com/doc/store/localization/languages) 
and append the target file to the **localizationFiles** [here](./src/typescript/frontend/utils/localization-manager.ts). 

### Locales are community managed

SteamClientHomebrew only maintains the english locales. If any changes are made to the english locales that wreck another language, contributors are responsible for updating them.

## Sponsors[^4]

Free code signing on Windows provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/)

[^1]: Installing and using Millennium
[^2]: View Millennium feature list
[^3]: Millennium language support 
[^4]: Code signing certificate sponsorship
