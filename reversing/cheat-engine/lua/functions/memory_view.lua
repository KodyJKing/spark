local module = {}

function module.createHexView(address)
    local mv = createMemoryView()
    mv.Caption = "Hex View: " .. hex(address)
    mv.HexadecimalView.Address = address
    mv.DisassemblerView.Height = 0
    mv.Panel1.Visible = false
    mv.Splitter1.Visible = false
    mv.Menu = nil
    local originalOnClose = mv.OnClose
    mv.OnClose = function(sender)
        mv.destroy()
        return originalOnClose(sender)
    end
    -- Attach Halo interpretation panel. Done after OnClose is set so that
    -- module.attach can chain it correctly when wrapping for cleanup.
    require("lua/halo/ui/memory_view_extensions").attach(mv)
    return mv
end

return module
