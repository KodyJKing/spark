-- Memory view extensions: Halo-specific 32-bit value interpretation panel.
-- Attaches to the main Memory View's HexadecimalView.OnByteSelect and shows
-- a label at the bottom of the window with human-readable interpretations of
-- the selected DWORD.

local module   = {}
local tags     = reloadPackage("lua/halo/engine/tags")
local entities = reloadPackage("lua/halo/engine/entities")



-- -------------------------------------------------------------------------
-- Interpreters
-- Each entry: { name = string, tryInterpret = function(dword) -> string|nil }
-- Return nil when the value does not match / cannot be resolved.
-- -------------------------------------------------------------------------
local interpreters = {
    {
        name = "Tag",
        tryInterpret = function(dword)
            local index = dword & 0xFFFF
            local tag   = tags.readTag(index)
            if not tags.tagExists(tag) then return nil end
            -- Validate the upper 16 bits match the tag's own id upper bits.
            -- tagID layout mirrors entity handle: upper 16 = generation id.
            if (dword >> 16) ~= ((tag.tagID >> 16) & 0xFFFF) then return nil end
            local path = tag.path or "?"
            return string.format("[%s] %s", tag.groupIDStr, path)
        end,
    },
    {
        name = "Entity",
        tryInterpret = function(dword)
            local ent = entities.getEntityByHandle(dword)
            if not ent then return nil end
            return string.format(
                "[%s] @%X hp=%.2f sh=%.2f  %s",
                ent.categoryName, ent.entityAddr, ent.health or 0, ent.shield or 0,
                ent.tagPath or "?"
            )
        end,
    },
}

-- -------------------------------------------------------------------------
-- Read the DWORD at a memory address; returns nil on failure.
-- -------------------------------------------------------------------------
local function readDword(address)
    local ok, v = pcall(readInteger, address)
    if not ok then return nil end
    if v == nil then return nil end
    return v & 0xFFFFFFFF
end

-- -------------------------------------------------------------------------
-- Attach the interpretation panel to any Memoryview instance.
-- Safe to call multiple times on the same instance (idempotent).
-- -------------------------------------------------------------------------
function module.attach(mv)
    if not mv then return end

    local hexView = mv.HexadecimalView
    if hexView.OnByteSelect then return end

    -- Panel anchored to the bottom of the form.
    local panel = createPanel(mv)
    panel.Align      = alBottom
    panel.Height     = 24
    panel.BevelOuter = bvNone
    panel.Visible    = false

    local lbl = createLabel(panel)
    lbl.Align   = alClient
    lbl.Caption = ""

    -- Hook selection events.
    hexView.OnByteSelect = function(sender, selStart, selStop)
        local dword = readDword(selStart)
        if not dword then
            panel.Visible = false
            return
        end

        local parts = {}
        for _, interp in ipairs(interpreters) do
            local ok, result = pcall(interp.tryInterpret, dword)
            if ok and result then
                table.insert(parts, interp.name .. ": " .. result)
            end
        end

        if #parts == 0 then
            panel.Visible = false
        else
            lbl.Caption   = string.format("0x%08X   |   ", dword) .. table.concat(parts, "   |   ")
            panel.Visible = true
        end
    end

end

-- Apply to the main memory view on load.
module.attach(getMemoryViewForm())

return module
