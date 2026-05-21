function reloadPackage(path)
    package.loaded[path] = nil
    return require(path)
end

dofile("lua/functions/globals.lua")
dofile("lua/highlight_entity_memview.lua")
dofile("lua/ipc.lua")
dofile("lua/functions/plot.lua")
dofile("lua/functions/halo1.lua")
dofile("lua/functions/string_scan.lua")
dofile("lua/register_address_symbol.lua")
dofile("lua/bp_freeze.lua")
-- dofile("lua/float_fuzz.lua")

local forms      = reloadPackage("lua/functions/forms")
local tagBrowser = reloadPackage("lua/halo/ui/tag_browser")

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Tag Browser", "Open"},
    function(item)
        item.OnClick = function(sender)
            tagBrowser.open()
        end
    end
)

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Reload", "Script"},
    function(item)
        item.OnClick = function(sender)
            reopenProcess()
            dofile("lua/tablemain.lua")
        end
    end
)

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Reload", "Cheat Engine"},
    function(item)
        item.OnClick = function(sender)
            reloadCheatEngineAndTable()
        end
    end
)
