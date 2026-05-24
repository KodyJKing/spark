-- Persistent floating form showing Ghidra decompiler variable annotations at the current RIP.
-- Recreated automatically if the user closes it.

local _form = nil
local _memo = nil

local function ensureForm()
    if _form ~= nil then return end

    _form = createForm(false)
    _form.Caption = "Ghidra hints"
    _form.Width = 500
    _form.Height = 280
    _form.onClose = function()
        _form:destroy()
        _form = nil
        _memo = nil
    end

    _memo = createMemo(_form)
    _memo.Align = alClient
    _memo.ReadOnly = true
    _memo.Font.Name = "Courier New"
    _memo.Font.Size = 10
    _memo.ScrollBars = ssVertical

    _form.show()
end

function ghUpdateUI(rip, ann)
    ensureForm()

    local lines = {}
    table.insert(lines, string.format("RIP  0x%016x", rip))
    table.insert(lines, string.rep("-", 56))

    local sorted = {}
    for reg, label in pairs(ann) do
        table.insert(sorted, { reg = reg, label = label })
    end
    table.sort(sorted, function(a, b) return a.reg < b.reg end)

    if #sorted == 0 then
        table.insert(lines, "(no named variables at this RIP)")
    else
        for _, e in ipairs(sorted) do
            table.insert(lines, string.format("%-6s  %s", e.reg, e.label))
        end
    end

    _memo.Lines.Text = table.concat(lines, "\n")

    if not _form.Visible then _form.show() end
end
