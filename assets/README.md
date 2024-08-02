## Introduction
This portion of the repository serves as a way to manage updates, plugins, and themes for Millennium. This plugin is automatically integrated into Millennium.


### Overview
- Manage Plugins in Steam Settings
- Manage updates for Plugins & Themes from Steam Settings
- Manage, and load custom user-made Themes
- Manage python 3.11.8 installation
- Manage Millennium Developer Tools
- Custom package manager for all python plugins

### Adding Languages

To add your spoken language to Millennium, fork this repository and place your language json (based on the [english locale](./src/locales/locales/english.json)) among the [current locales](./src/locales/locales/). Name the file `{your_language}.json` and append the target file to the `localizationFiles` in [this file](./src/locales/index.ts).

Millennium is only maintaining the english language, and if any changes are made to the english locales that don't reflect on a target language, contributors are responsible for updating them. 

### Contributing
Clone this repository place it in your plugins folder and start Steam. You can now make changes to this library. Added changes to the `backend` should also take platform type into consideration as this library will eventually support Linux, OSX, and Windows >= 10

### Building 

```
git clone https://github.com/SteamClientHomebrew/Millennium.git
cd Millennium/assets
npm install
npm run dev
```
