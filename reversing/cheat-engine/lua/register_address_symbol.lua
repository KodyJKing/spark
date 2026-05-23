
local function getMemoryOperandAt(address)
    local ok, disasm = pcall(disassemble, address)
    if not ok or not disasm then return nil end

    local _, opcode, _, _ = splitDisassembledString(disasm)
    if not opcode then return nil end

    local operand = opcode:match("%[(.-)%]")
    if not operand or operand == "" then return nil end

    return operand
end

local function registerSymbolAt(name, address)
    local relativeAddress = getNameFromAddress(address)
    registerSymbol(name, relativeAddress)
end

local memoryForm = getMemoryViewForm()
if memoryForm then
    local forms = require("lua/functions/forms")

    -- Register symbol for selected address.
    forms.addMenuItem(
        memoryForm.debuggerpopup.Items,
        {"Symbols", "Add address"},
        function(item)
            item.OnClick = function(sender)
                local address = memoryForm.DisassemblerView.SelectedAddress
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                local symbolName = inputQuery("Register Symbol", "Enter symbol name for address 0x" .. string.format("%X", address) .. ":", "")
                if symbolName and symbolName ~= "" then
                    -- Register the symbol in Cheat Engine's symbol list
                    registerSymbolAt(symbolName, address)
                else
                    messageDialog("Invalid symbol name.", mtError, mbOK)
                end
            end
        end
    )

    -- Register memory operand as a symbol.
    forms.addMenuItem(
        memoryForm.debuggerpopup.Items,
        {"Symbols", "Add memory operand"},
        function(item)
            item.OnClick = function(sender)
                local address = memoryForm.DisassemblerView.SelectedAddress
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                local operand = getMemoryOperandAt(address)
                if not operand then
                    messageDialog("Selected instruction does not have a valid memory operand.", mtError, mbOK)
                    return
                end

                local symbolName = inputQuery("Register Symbol", "Enter symbol name for operand '" .. operand .. "' at address 0x" .. string.format("%X", address) .. ":", "")
                if symbolName and symbolName ~= "" then
                    -- Register the symbol in Cheat Engine's symbol list
                    registerSymbolAt(symbolName, operand)
                else
                    messageDialog("Invalid symbol name.", mtError, mbOK)
                end
            end
        end
    )

    -- Remove symbol for selected address.
    forms.addMenuItem(
        memoryForm.debuggerpopup.Items,
        {"Symbols", "Remove address"},
        function(item)
            item.OnClick = function(sender)
                local address = memoryForm.DisassemblerView.SelectedAddress
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                local name = getNameFromAddress(address)
                if not name then
                    messageDialog(string.format("Address 0x%X does not have a registered symbol.", address), mtInformation, mbOK)
                    return
                end

                local response = messageDialog(string.format("Are you sure you want to unregister symbol '%s' for address 0x%X?", name, address), mtConfirmation, mbYesNo)
                if response == mrYes then
                    unregisterSymbol(name)
                end
            end
        end
    )

    -- Remove symbol for memory operand.
    forms.addMenuItem(
        memoryForm.debuggerpopup.Items,
        {"Symbols", "Remove memory operand"},
        function(item)
            item.OnClick = function(sender)
                local address = memoryForm.DisassemblerView.SelectedAddress
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                local operand = getMemoryOperandAt(address)
                if not operand then
                    messageDialog("Selected instruction does not have a valid memory operand.", mtError, mbOK)
                    return
                end

                local name = getNameFromAddress(operand)
                if not name then
                    messageDialog(string.format("Operand '%s' does not have a registered symbol.", operand), mtInformation, mbOK)
                    return
                end

                local response = messageDialog(string.format("Are you sure you want to unregister symbol '%s' for operand '%s'?", name, operand), mtConfirmation, mbYesNo)
                if response == mrYes then
                    unregisterSymbol(name)
                end
            end
        end
    )
end
