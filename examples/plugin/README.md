> [!CAUTION]
> This plugin example and other related documentation is not yet finished. You will not receive support for development until everything is ready. This plugin template may not be fully functional, and critical portions of it may be outdated. Once this example is ready, this message will be removed

## Plugin Template
A plugin template for Millennium providing a basic boilerplate to help get started. You'll need a decent understanding in python, and typescript (superset of javascript)
<br>

## Prerequisites
- **[Millennium](https://github.com/SteamClientHomebrew/Millennium)**

## Setting up
```ps1
git clone https://github.com/SteamClientHomebrew/PluginTemplate.git
cd PluginTemplate
pnpm install
```

## Building
```
pnpm run dev
```

Then ensure your plugin template is in your plugins folder. 
`%MILLENNIUM_PATH%/plugins`, and select it from the "Plugins" tab within steam, or run `millennium plugins enable your_plugin_internal_name`

#### Note:
**MILLENNIUM_PATH** =
* Steam Path (ex: `C:\Program Files (x86)\Steam`) (Windows)
* `~/.millennium` (Unix)

## Next Steps

https://docs.steambrew.app/developers/plugins/learn

<!-- ## Deploying
```ps1
pnpm run build
``` -->
