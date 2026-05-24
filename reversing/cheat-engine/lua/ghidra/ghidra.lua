local instances = {}
dofile("lua/ghidra/ghidra_ui.lua")

function ghLoad(force)
    if not force and #instances > 0 then return end
    instances = dofile("lua/ghidra/scripts/instances.lua")
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
        print("  (no variables found for RIP 0x" .. string.format("%x", rip) .. ")")
    end
end

function debugger_onBreakpoint()
    local ann = ghAnnotate(RIP)
    ghUpdateUI(RIP, ann)
end
