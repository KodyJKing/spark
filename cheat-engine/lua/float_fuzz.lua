--[[
    Poll a thread for floating point instructions. Then binary search for interesting instructions by adding noise to instructions in bulk.
]]--

-- Regularly stop a thread and add noise to floating point registers.
local function float_fuzz(threadId)
    local form = createForm(false)
    form.Caption = "Float Fuzzer"
    form.Width = 200
    form.Height = 100

    local FUZZ_MAX = 1000.0
    local FUZZ_MIN = 0.00001
    local NOISE_SCALE = 10
    local function fuzzable(value)
        local abs = math.abs(value)
        return abs > FUZZ_MIN and abs < FUZZ_MAX
    end

    local function fuzzXmmReg(i)
        local ptr = debug_getXMMPointer(i)
        local value = readFloatLocal(ptr)
        if fuzzable(value) then
            local fuzz = (math.random() - 0.5) * 2 * value * NOISE_SCALE
            writeFloatLocal(ptr, value + fuzz)
        end
    end

    debugger_onBreakpoint = function ()
        debug_getContext(true)
        for i=0,15 do
            fuzzXmmReg(i)
        end
        debug_continueFromBreakpoint(co_run)
        debug_setContext(true)
    end

    local timer = createTimer(form)
    timer.Interval = 1
    timer.OnTimer = function()
        debug_breakThread(threadId)
    end

    local function destroy()
        timer:destroy()
        form:destroy()
        debugger_onBreakpoint = nil
    end

    form.OnClose = function()
        destroy()
    end

    form:show()

    return {
        destroy = destroy
    }
end

local forms = require("lua/functions/forms")
forms.addMenuItem(
    getMainForm().Menu.Items,
    {"Float Fuzzer"},
    function(item)
        item.OnClick = function(sender)
            -- Prompt for thread ID
            local threadIdStr = inputQuery("Float Fuzzer", "Enter the thread ID to fuzz (hexadecimal):", "")
            if not threadIdStr then return end
            local threadId = tonumber(threadIdStr, 16)
            if not threadId then
                forms.showMessage("Invalid thread ID.", "Float Fuzzer")
                return
            end
    
            float_fuzz(threadId)
        end
    end
)
