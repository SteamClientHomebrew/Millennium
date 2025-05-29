## Introduction

This sub-project is responsible for everything you see from within the Steam Client. if a UI change has been made, it was made in here.

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
