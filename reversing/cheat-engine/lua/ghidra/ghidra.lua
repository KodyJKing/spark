local ChunkIndex = dofile("lua/ghidra/chunk_index.lua")

local instances = {}

local functionIndex = nil
local globalIndex  = nil

dofile("lua/ghidra/ghidra_ui.lua")

function ghLoad(force)
    if force or #instances == 0 then 
        instances = dofile("lua/ghidra/scripts/out/instances.lua")
    end
    if force or functionIndex == nil then
        local functions = dofile("lua/ghidra/scripts/out/functions.lua")
        functionIndex = ChunkIndex.new(functions, function(f) return f.start end)
    end
    if force or globalIndex == nil then
        local globals = dofile("lua/ghidra/scripts/out/globals.lua")
        globalIndex = ChunkIndex.new(globals, function(g) return g.address end)
    end
end

function ghDumpInstances()
    ghLoad(false)
    for _, v in ipairs(instances) do
        print(string.format("reg=%s def=0x%x lastUse=0x%x name=%s type=%s kind=%s",
            v.reg, v.def, v.lastUse, v.name, v.type, v.kind))
    end
end

-- Returns a table mapping register name -> "varname : type (kind)" for all
-- variables whose live range contains rip.
function ghAnnotate(rip)
    if rip == nil then rip = RIP end
    ghLoad(false)
    
    local out = {}
    for _, v in ipairs(instances) do
        if rip >= v.def and rip <= v.lastUse then
            -- Later entries (higher def) win, overwriting earlier SSA versions
            -- of the same register that may have stale ranges overlapping rip.
            local existing = out[v.reg]
            if not existing or v.def > (out[v.reg .. "_def"] or 0) then
                out[v.reg] = v.name .. " : " .. v.type .. " (" .. v.kind .. ")"
                out[v.reg .. "_def"] = v.def
            end
        end
    end
    -- Strip the internal _def tracking keys before returning
    for k in pairs(out) do
        if k:sub(-4) == "_def" then out[k] = nil end
    end
    return out
end

function ghAnnotateAddress(rip)
    if rip == nil then rip = RIP end
    ghLoad(false)

    -- Globals take priority: exact address match.
    local g = globalIndex:lookupExact(rip)
    if g ~= nil then return g.name end

    -- Fall back to function range lookup.
    local f = functionIndex:lookupRange(rip)
    if f == nil then return nil end

    local offset = rip - f.start
    if offset == 0 then
        return f.name
    else
        return f.name .. string.format("+0x%X", offset)
    end
end

-- Prints annotations for the given rip to the console.
function ghDump(rip)
    if rip == nil then rip = RIP end
    ghLoad(false)

    local ann = ghAnnotate(rip)
    local count = 0
    for reg, label in pairs(ann) do
        print(string.format("  %-6s  %s", reg, label))
        count = count + 1
    end
    if count == 0 then
        print("  (no variables found for RIP 0x" .. string.format("%X", rip) .. ")")
    end
end

local id = registerAddressLookupCallback(ghAnnotateAddress)
table.insert(reloadFns, function() unregisterAddressLookupCallback(id) end)
