local logger = require("logger")
local millennium = require("millennium")

function test_frontend_message_callback(message, status, count)
    logger:info("test_frontend_message_callback called")
    logger:info("Received args: " .. table.concat({ message, tostring(status), tostring(count) }, ", "))

    return true
end

local function on_load()
    logger:info("Comparing millennium version: " .. millennium.version())

    -- We are running a Millennium version > 2.29.3
    local target_version = "2.29.3"
    if millennium.cmp_version(millennium.version(), target_version) == 1 then
        logger:info("Running Millennium > " .. target_version)
    end

    logger:info("Example plugin loaded with Millennium version " .. millennium.version())

    -- Set a default greeting if one doesn't exist yet
    local greeting = millennium.config.get("greeting")
    if greeting == nil then
        millennium.config.set("greeting", "Hello from Lua!")
        logger:info("Set default greeting")
    else
        logger:info("Current greeting: " .. tostring(greeting))
    end

    -- Listen for config changes (from frontend, MEP, or anywhere)
    millennium.config.on_change(function(key, value)
        logger:info("Config changed: " .. key .. " = " .. tostring(value))
    end)

    millennium.ready()
end

-- Called when your plugin is unloaded. This happens when the plugin is disabled or Steam is shutting down.
-- NOTE: If Steam crashes or is force closed by task manager, this function may not be called -- so don't rely on it for critical cleanup.
local function on_unload()
    logger:info("Plugin unloaded")
end

-- Called when the Steam UI has fully loaded.
local function on_frontend_loaded()
    logger:info("Frontend loaded")
    local result = millennium.call_frontend_method("SomeClass.method", { 18, "USA", false })
    logger:info(result)
end

return {
    on_frontend_loaded = on_frontend_loaded,
    on_load = on_load,
    on_unload = on_unload,

    -- patches let you directly override content from steam's js files as if they served with it.
    -- effectively, this means you can trampoline, hook, block, or entirely change the functionality of steam's js files.
    -- If you check the network tab on the chrome inspector, you'll see this file has directly served with this patch inside of it,
    -- Millennium did all the hard lifting :)
    patches = {
        {
            -- this find segment dictates the content you are able to edit. it essentially casts a net over a portion of the file content
            -- and tells Millennium you'll be editing it. This helps with optimization, and preventing Millennium from selecting content you didn't me to select.
            -- Uses RE2 regex syntax matched against file content.
            find =
            [["#Menu_Account"\):\(0,\w+\.jsxs\)\("div",\{className:\w+\(\)\.SteamButton,children:\[\(0,\w+\.jsx\)\(\w+\.SteamLogo]],

            -- Tell Millennium to only target files starting with "chunk" as this is the file we are concerned with.
            -- This helps Millennium optimize your selector, and prevent accidentally patching files you didn't mean to.
            file = [[chunk~[0-9a-f]+\.js]],

            -- All transforms are handled with RE2 regex.
            transforms = {
                {
                    -- Capture the jsx runtime fn name (\1) so the replace stays correct even if it's minified differently.
                    match = [[\(0,(\w+\.jsx)\)\(\w+\.SteamLogo]],
                    -- #{{self}} is a macro that denotes your plugins frontend instance.
                    -- hookedSettingsIcon() will be called on your frontend now.
                    -- Make sure to "Millennium.exposeObj({ hookedSettingsIcon });" first, otherwise the function will be private by default.
                    replace = [[(0,\1)(#{{self}}?.hookedSettingsIcon?.().SteamButton||(()=>null)]], -- fallback to no-op when plugin isn't ready yet to avoid React error #130
                }
                -- this is a list, you can add more elements
            }
        }
        -- this is a list, you can add more elements
    }
}
