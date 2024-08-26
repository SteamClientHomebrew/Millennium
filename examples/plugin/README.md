> [!CAUTION]
> This plugin example and other related documentation is not yet finished. You will not receive support for development until everything is ready. This plugin template may not be fully functional, and critical portions of it may be outdated. Once this example is ready, this message will be removed

## Plugin Template
A plugin template for Millennium providing a basic boilerplate to help get started. You'll need a decent understanding in python, and typescript (superset of javascript)
<br>

## Prerequisites
- **[Millennium](https://github.com/SteamClientHomebrew/Millennium)**

## Setting up
```ps1
git clone --depth=1 https://github.com/SteamClientHomebrew/Millennium 
mv ./Millennium/examples/plugin ./plugin_template 
rm -rf Millennium

cd plugin_template
pnpm install
```

## Building
```
pnpm run dev
```

Then ensure your plugin template is in your plugins folder. 
`%MILLENNIUM_PATH%/plugins/plugin_template`, and select it from the "Plugins" tab within steam, or run `millennium plugins enable plugin_template`

#### Note:
**MILLENNIUM_PATH** =
* Steam Path (ex: `C:\Program Files (x86)\Steam`) (Windows)
* `~/.millennium` (Unix)

## Next Steps

https://docs.steambrew.app/developers/plugins/learn