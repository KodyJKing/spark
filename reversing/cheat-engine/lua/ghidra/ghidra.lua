local instances = {}
local functions = {}

local functionChunks = {}

dofile("lua/ghidra/ghidra_ui.lua")

function chunkBy(values, keyFunc)
    local out = {}
    for _, v in ipairs(values) do
        local key = keyFunc(v)
        if out[key] == nil then out[key] = {} end
        table.insert(out[key], v)
    end
    return out
end

function getFunctionChunkId(rip)
    return math.floor(rip / 0x4000)
end

function ghLoad(force)
    if force or #instances == 0 then 
        instances = dofile("lua/ghidra/scripts/out/instances.lua")
    end
    if force or #functions == 0 then
        functions = dofile("lua/ghidra/scripts/out/functions.lua")
        -- Chunk into 16KB groups by start address for faster lookup in ghAnnotateFunction.
        functionChunks = chunkBy(functions, function(f) return getFunctionChunkId(f.start) end)
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

function ghAnnotateFunction(rip)
    if rip == nil then rip = RIP end
    ghLoad(false)

    local chunk = functionChunks[getFunctionChunkId(rip)]
    if chunk == nil then return nil end

    for _, f in ipairs(chunk) do
        if rip >= f.start and rip <= f.stop then
            local offset = rip - f.start
            if offset == 0 then
                return f.name
             else
                 return f.name .. string.format("+0x%X", offset)
             end
        end
    end
    return nil
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

local id = registerAddressLookupCallback(ghAnnotateFunction)
table.insert(reloadFns, function() unregisterAddressLookupCallback(id) end)
