-- Break on write to a specific address and write the original value back after each write.
function bp_freeze(address, size)
    local originalValue = readBytes(address, size, true)
    debug_setBreakpoint(address, size, bptWrite, function()
        writeBytes(address, originalValue, true)
        debug_continueFromBreakpoint(co_run)
    end)
end

-- Add noise to a specific address on each write. Useful for visually identifying the role of a dependent value in the game.
-- Floating-point only.
function bp_add_noise(address, noiseScale, relativeNoise)
    if relativeNoise == nil then relativeNoise = false end

    debug_setBreakpoint(address, 4, bptWrite, function()
        local value = readFloat(address)
        local randomValue = (math.random() * 2 - 1) * noiseScale;
        if relativeNoise then
            randomValue = randomValue * value
        end
        value = value + randomValue
        writeFloat(address, value)
        debug_continueFromBreakpoint(co_run)
    end)
end

function bp_slider(address)
    local value = readFloat(address)
    local min = -1
    local max = 1

    local sliderForm = createForm()
    sliderForm.Caption = string.format("Slider for 0x%X", address)
    sliderForm.Width = 300
    sliderForm.Height = 60

    local minInput = createEdit(sliderForm)
    minInput.Align = alLeft
    minInput.Text = tostring(min)

    local maxInput = createEdit(sliderForm)
    maxInput.Align = alRight
    maxInput.Text = tostring(max)

    local trackBar = createTrackBar(sliderForm)
    trackBar.Align = alBottom
    trackBar.Min = 0
    trackBar.Max = 10
    trackBar.Position = 0

    function updateValue()
        value = min + trackBar.Position / trackBar.Max * (max - min)
    end

    trackBar.OnChange = function()
        updateValue()
    end
    minInput.OnChange = function()
        local newMin = tonumber(minInput.Text)
        if newMin then
            min = newMin
            updateValue()
        end
    end
    maxInput.OnChange = function()
        local newMax = tonumber(maxInput.Text)
        if newMax then
            max = newMax
            updateValue()
        end
    end

    debug_setBreakpoint(address, 4, bptWrite, function()
        writeFloat(address, value)
        debug_continueFromBreakpoint(co_run)
    end)

    sliderForm.Show()
    sliderForm.OnClose = function()
        debug_removeBreakpoint(address)
        sliderForm.destroy()
    end
end

-- UI Integration

local sizes = {
    dtByte = 1, 
    dtByteDec = 1, 
    dtWord = 2, 
    dtWordDec = 2,
    dtDword = 4,
    dtDwordDec = 4,
    dtQword = 8,
    dtQwordDec = 8,
    dtSingle = 4,
    dtDouble = 8
}

local memoryForm = getMemoryViewForm()
if memoryForm then
    local forms = require("lua/functions/forms")
    local popup = memoryForm.memorypopup

    -- Add "Freeze on write" option to memory view context menu.
    forms.addMenuItem(
        popup.Items,
        {"Freeze"},
        function(item)
            item.OnClick = function(sender)
                local hexView = memoryForm.HexadecimalView
                if not hexView.HasSelection then
                    messageDialog("No address selected.", mtError, mbOK)
                    return
                end

                local address = hexView.SelectionStart
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                local size = sizes[hexView.DisplayType]

                bp_freeze(address, size)
            end
        end
    )

    -- Add noise.
    forms.addMenuItem(
        popup.Items,
        {"Add Noise"},
        function(item)
            item.OnClick = function(sender)
                local hexView = memoryForm.HexadecimalView
                if not hexView.HasSelection then
                    messageDialog("No address selected.", mtError, mbOK)
                    return
                end

                -- DisplayType must be a floating-point type for noise to work correctly.
                if hexView.DisplayType ~= "dtSingle" then
                    messageDialog("Only floating point is supported", mtError, mbOK)
                    return
                end

                local address = hexView.SelectionStart
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                -- Ask user for noise scale.
                local noiseScaleStr = inputQuery("Noise Scale", "Enter noise scale (e.g. 0.1 for ±10% noise):", "0.1")
                local noiseScale = tonumber(noiseScaleStr)
                if not noiseScale then
                    messageDialog("Invalid noise scale.", mtError, mbOK)
                    return
                end

                bp_add_noise(address, noiseScale)
            end
        end
    )

    -- Create slider.
    forms.addMenuItem(
        popup.Items,
        {"Create Slider"},
        function(item)
            item.OnClick = function(sender)
                local hexView = memoryForm.HexadecimalView
                if not hexView.HasSelection then
                    messageDialog("No address selected.", mtError, mbOK)
                    return
                end

                -- DisplayType must be a floating-point type for slider to work correctly.
                if hexView.DisplayType ~= "dtSingle" then
                    messageDialog("Only floating point is supported", mtError, mbOK)
                    return
                end

                local address = hexView.SelectionStart
                if not address then
                    messageDialog("No instruction selected.", mtError, mbOK)
                    return
                end

                bp_slider(address, min, max, step)
            end
        end
    )
end
