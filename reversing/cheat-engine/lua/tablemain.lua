function reloadPackage(path)
    package.loaded[path] = nil
    return require(path)
end

-- Add cleanup functions to this table.
reloadFns = {}

dofile("lua/functions/globals.lua")
dofile("lua/highlight_entity_memview.lua")
dofile("lua/ipc.lua")
dofile("lua/functions/plot.lua")
dofile("lua/functions/halo1.lua")
dofile("lua/functions/string_scan.lua")
dofile("lua/register_address_symbol.lua")
dofile("lua/bp_freeze.lua")
dofile("lua/ghidra/ghidra.lua")
-- dofile("lua/float_fuzz.lua")

local forms         = reloadPackage("lua/functions/forms")
local tagBrowser    = reloadPackage("lua/halo/ui/tag_browser")
local entityBrowser = reloadPackage("lua/halo/ui/entity_browser")
reloadPackage("lua/halo/ui/memory_view_extensions")

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Tag Browser"},
    function(item)
        item.OnClick = function(sender)
            tagBrowser.open()
        end
    end
)

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Entity Browser"},
    function(item)
        item.OnClick = function(sender)
            entityBrowser.open()
        end
    end
)

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Reload", "Script"},
    function(item)
        item.OnClick = function(sender)
            -- reopenProcess()
            for _, fn in ipairs(reloadFns) do
                fn()
            end
            reloadFns = {}
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

forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Halo CE", "Copy Halo Base Address"},
    function(item)
        item.OnClick = function(sender)
            local haloBase = getAddress("halo1.dll")
            if haloBase then
                writeToClipboard(string.format("0x%X", haloBase))
            else
                showMessage("Failed to find Halo base address.")
            end
        end
    end
)
