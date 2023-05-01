# Join the support server and see updates
https://discord.gg/MXMWEQKgJF

# Millennium Steam Patcher for Windows 
Apply steam skins Steam after the 2023-04-27 Chromium UI update

## How to install
drag all the DLL files into the steam directory on your WINDOWS computer presumably `C:\Program Files (x86)\Steam`.
from there run steam with the command line arguments `-cef-enable-debugging`

## How to skin my library
going off the template in the reposity, drag the skins folder into steamui in the steam directory `C:\Program Files (x86)\Steam\steamui`
and from there follow the config.json settings in order to style your library

the settings.json is what controlls whether or not the console shows and what skin you have selected.

## Bugs
this is just a prototype and its bound to have bugs, report them if possible to me `ShadowMonster#5099` on discord.
as of now any discrepancies in any of the json files MAY cause a crash of the steam library to not skin.
valid your json before putting it into the files `setting.json` or `config.json`

## compiling for yourself?
you need to install x86 version of the following libraries from vcpkg and compile in visual studio 2022
- `websocketpp`
- `rapidjson`
- `nlohmann`
- `boost`

no further help will be provided on how to compile. 
