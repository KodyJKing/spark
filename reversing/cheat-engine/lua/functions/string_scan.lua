-- Returns true if >= threshold fraction of the string's bytes are [A-Za-z0-9].
local function is_predominantly_alphanumeric(str, threshold)
    threshold = threshold or 0.5
    local len = #str
    if len < 3 then return false end
    local count = 0
    for i = 1, len do
        local b = str:byte(i)
        if (b >= 65 and b <= 90) or   -- A-Z
           (b >= 97 and b <= 122) or  -- a-z
           (b >= 48 and b <= 57) then -- 0-9
            count = count + 1
        end
    end
    return count / len >= threshold
end

-- Resolves a bracket expression (e.g. "7FF800001234" or "Module+offset") to an address.
local function resolve_lea_target(expr)
    if not expr or expr == "" then return nil end

    -- Try plain hex string first
    local hex = expr:match("^%s*([0-9a-fA-F]+)%s*$")
    if hex then
        local addr = tonumber(hex, 16)
        if addr and addr > 0x10000 then return addr end
    end

    -- Let CE's symbol/expression resolver handle module+offset, rip-relative, etc.
    local prev = errorOnLookupFailure(false)
    local ok, addr = pcall(getAddress, expr)
    errorOnLookupFailure(prev)
    if ok and type(addr) == "number" and addr > 0x10000 then
        return addr
    end

    return nil
end

-- Disassemble a function at given address and recursively scans for string references.
-- Returns a table of {address, str, from} entries for every predominantly alpha-numeric
-- string that a "lea" instruction points to within the function (and its callees).
function string_scan(address, max_depth, visited)
    if max_depth == nil then max_depth = 3 end
    if visited  == nil then visited  = {} end

    local results = {}
    if not address or address == 0 then return results end
    if visited[address] then return results end
    visited[address] = true

    local current = address

    for _ = 1, 2000 do
        local ok, disasm = pcall(disassemble, current)
        if not ok or not disasm then break end

        local _, opcode, _, _ = splitDisassembledString(disasm)
        if not opcode then break end

        local sz = getInstructionSize(current)
        if not sz or sz <= 0 then break end

        local op_lower = opcode:lower()

        -- Stop scanning at function return
        if op_lower:match("^ret") then break end

        -- ----------------------------------------------------------------
        -- LEA: load effective address – look for string literals
        -- ----------------------------------------------------------------
        if op_lower:match("^lea%s") then
            local bracket_expr = opcode:match("%[(.-)%]")
            if bracket_expr then
                local ref_addr = resolve_lea_target(bracket_expr)
                if ref_addr then
                    local ok2, str = pcall(readString, ref_addr, 512)
                    if ok2 and str and #str >= 3 and is_predominantly_alphanumeric(str) then
                        table.insert(results, {
                            address = ref_addr,
                            str     = str,
                            from    = current,
                        })
                    end
                end
            end
        end

        -- ----------------------------------------------------------------
        -- CALL: recurse into the callee (up to max_depth)
        -- ----------------------------------------------------------------
        if max_depth > 0 and op_lower:match("^call%s") then
            local target_str = opcode:match("[Cc]all%s+(.+)$")
            if target_str then
                target_str = target_str:match("^%s*(.-)%s*$")  -- trim whitespace
                local call_target = resolve_lea_target(target_str)
                if call_target and not visited[call_target] then
                    local sub = string_scan(call_target, max_depth - 1, visited)
                    for _, v in ipairs(sub) do
                        table.insert(results, v)
                    end
                end
            end
        end

        current = current + sz
    end

    return results
end

-- UI Intregration:

local memoryForm = getMemoryViewForm()
if memoryForm then
    local forms = require("lua/functions/forms")
    forms.addMenuItem(
        memoryForm.debuggerpopup.Items,
        {"Tools", "Find referenced strings"},
        function(item)
            item.OnClick = function(sender)
                local address = memoryForm.DisassemblerView.SelectedAddress
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end
        
                local results = string_scan(address)
                if #results == 0 then
                    messageDialog("No strings found.", mtInformation, mbOK)
                    return
                end
        
                local msg = "Found the following strings:\n\n"
                for _, v in ipairs(results) do
                    msg = msg .. string.format("0x%X: \"%s\" (referenced from 0x%X)\n", v.address, v.str, v.from)
                end
        
                print(msg)
            end
        end
    )
end

return {
    string_scan = string_scan,
}
