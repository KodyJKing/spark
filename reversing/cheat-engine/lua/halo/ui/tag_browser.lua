-- Tag Browser form for Cheat Engine.
-- Rudimentary UI equivalent of smc64/src/haloce/ui/TagBrowser.cpp

local module = {}

local tags       = reloadPackage("lua/halo/engine/tags")
local memoryView = reloadPackage("lua/functions/memory_view")

local PAGE_SIZE = 100

-- Column definitions, ordered left-to-right.
-- Stored at module level so right-click copy (per column) can reference them.
-- ListView mapping: COLUMNS[1] -> item.Caption, COLUMNS[n] -> item.SubItems[n-2]
module.COLUMNS = {
    { caption = "Index",        width = 55  },
    { caption = "TagID",        width = 85  },
    { caption = "GroupID",      width = 65  },
    { caption = "PDataAddress", width = 135 },
    { caption = "DataAddress",  width = 135 },
    { caption = "Path",         width = 560 },
}

local state = {
    form    = nil,
    results = {},
    page    = 0,
}

local function pageCount()
    return math.max(1, math.ceil(#state.results / PAGE_SIZE))
end

local function clampPage()
    state.page = math.max(0, math.min(state.page, pageCount() - 1))
end

local function populateList(lv, pageLabel)
    lv.Items.Clear()
    clampPage()
    local base = state.page * PAGE_SIZE
    for i = 1, PAGE_SIZE do
        local tag = state.results[base + i]
        if not tag then break end
        local item = lv.Items.Add()
        -- Column 0
        item.Caption = tostring(tag.index)
        -- Columns 1-4 (SubItems 0-3)
        item.SubItems.Add(string.format("%X", tag.tagID))
        item.SubItems.Add(tag.groupIDStr)
        item.SubItems.Add(string.format("%X", tag.pDataAddress or 0))
        item.SubItems.Add(string.format("%X", tag.absDataAddress or 0))
        item.SubItems.Add(tag.path or "")
    end
    pageLabel.Caption = string.format("Page %d / %d", state.page + 1, pageCount())
end

function module.open()
    -- Reuse existing form if still alive
    if state.form then
        local ok = pcall(function() state.form:show() end)
        if ok then return end
        state.form = nil
    end

    local form = createForm(false)
    state.form = form
    form.Caption  = "Tag Browser"
    form.Width    = 980
    form.Height   = 620
    form.Position = poScreenCenter

    form.OnClose = function(sender)
        state.form:destroy()
        state.form = nil
    end

    -- ------------------------------------------------------------------
    -- Row 1: group filter combo + path search input + search button
    -- ------------------------------------------------------------------
    local combo = createComboBox(form)
    combo.Left  = 8
    combo.Top   = 8
    combo.Width = 165
    combo.Style = 2  -- csDropDownList: read-only dropdown
    for _, g in ipairs(tags.GROUP_IDS) do
        combo.Items.Add(g.name)
    end
    combo.ItemIndex = 0

    local searchEdit = createEdit(form)
    searchEdit.Left  = combo.Left + combo.Width + 4
    searchEdit.Top   = 8
    searchEdit.Width = 680

    local searchBtn = createButton(form)
    searchBtn.Left    = searchEdit.Left + searchEdit.Width + 4
    searchBtn.Top     = 8
    searchBtn.Width   = 80
    searchBtn.Height  = 25
    searchBtn.Caption = "Search"

    -- ------------------------------------------------------------------
    -- Row 2: result count label
    -- ------------------------------------------------------------------
    local countLabel = createLabel(form)
    countLabel.Left    = 8
    countLabel.Top     = combo.Top + combo.Height + 6
    countLabel.Caption = "0 results"

    -- ------------------------------------------------------------------
    -- Row 3: results ListView
    -- ------------------------------------------------------------------
    local lv = createListView(form)
    lv.Left        = 8
    lv.Top         = countLabel.Top + countLabel.Height + 4
    lv.Width       = form.ClientWidth - 16
    lv.ViewStyle   = vsReport
    lv.ReadOnly    = true
    lv.RowSelect   = true
    lv.GridLines   = true
    lv.HideSelection = false

    for _, col in ipairs(module.COLUMNS) do
        local c = lv.Columns.Add()
        c.Caption = col.caption
        c.Width   = col.width
    end

    -- ------------------------------------------------------------------
    -- Row 4: pagination (positioned by layoutBottom)
    -- ------------------------------------------------------------------
    local prevBtn = createButton(form)
    prevBtn.Caption = "< Prev"
    prevBtn.Width   = 70

    local pageLabel = createLabel(form)
    pageLabel.Caption = "Page 1 / 1"

    local nextBtn = createButton(form)
    nextBtn.Caption = "Next >"
    nextBtn.Width   = 70

    local function layoutBottom()
        local y = form.ClientHeight - 30
        prevBtn.Top    = y
        prevBtn.Left   = 8
        pageLabel.Top  = y + 4
        pageLabel.Left = prevBtn.Left + prevBtn.Width + 8
        nextBtn.Top    = y
        nextBtn.Left   = pageLabel.Left + pageLabel.Width + 8
        lv.Height      = y - lv.Top - 6
        lv.Width       = form.ClientWidth - 16
    end
    layoutBottom()

    -- ------------------------------------------------------------------
    -- Event handlers
    -- ------------------------------------------------------------------
    local function doSearch()
        local groupIndex = combo.ItemIndex          -- 0-based CE index
        local searchText = searchEdit.Text:lower()
        local groupEntry = tags.GROUP_IDS[groupIndex + 1]  -- Lua 1-based

        local filterFn = function(tag)
            -- Group ID filter (matches group, parent, or grandparent)
            if groupEntry and groupEntry.fourcc ~= 0 then
                if  tag.groupID            ~= groupEntry.fourcc
                and tag.parentGroupID      ~= groupEntry.fourcc
                and tag.grandparentGroupID ~= groupEntry.fourcc
                then
                    return false
                end
            end
            -- Path substring filter
            if searchText ~= "" then
                local path = (tag.path or ""):lower()
                local tagID = string.format("%X", tag.tagID or 0):lower()
                if not path:find(searchText, 1, true)
                and not tagID:find(searchText, 1, true)
                then
                    return false
                end
            end

            

            return true
        end

        state.results = tags.enumerateTags(filterFn)
        state.page    = 0
        countLabel.Caption = string.format("%d results", #state.results)
        populateList(lv, pageLabel)
    end

    searchBtn.OnClick = function(sender)
        doSearch()
    end

    searchEdit.OnKeyDown = function(sender, key)
        if key == 13 then doSearch() end
    end

    prevBtn.OnClick = function(sender)
        state.page = state.page - 1
        clampPage()
        populateList(lv, pageLabel)
    end

    nextBtn.OnClick = function(sender)
        state.page = state.page + 1
        clampPage()
        populateList(lv, pageLabel)
    end

    -- Double-click: open CE hex view of the tag's data block
    lv.OnDblClick = function(sender)
        local item = lv.Selected
        if not item then return end
        -- SubItems[3] = DataAddress column (0-based SubItems index)
        local addrStr = item.SubItems[3]
        local addr = tonumber(addrStr, 16)
        if not addr or addr == 0 then return end
        local mv = memoryView.createHexView(addr)
        mv.Caption = "Tag Data: " .. (item.SubItems[4] or "")
        mv:show()
    end

    -- Context menu
    local popup = createPopupMenu(lv)

    local bpReadItem = createMenuItem(popup)
    bpReadItem.Caption = "Set read breakpoint on PDataAddress"
    bpReadItem.OnClick = function(sender)
        local item = lv.Selected
        if not item then return end
        local addr = tonumber(item.SubItems[2], 16)  -- SubItems[2] = PDataAddress
        if not addr or addr == 0 then
            print("Tag Browser: selected tag has no valid PDataAddress")
            return
        end
        debug_setBreakpoint(addr, 4, bptAccess)
        print(string.format("Tag Browser: read breakpoint set at %X (PDataAddress)", addr))
    end
    popup.Items.Add(bpReadItem)

    lv.PopupMenu = popup

    -- Copy cell on right-click
    lv.OnMouseDown = function(sender, button, x, y)
        if button == 2 then return end

        local columnIndex = 0
        local xLeft = x
        while xLeft > 0 and columnIndex < #module.COLUMNS do
            local col = lv:getColumns()[columnIndex]
            local colWidth = col.Width
            if xLeft <= colWidth then break end
            xLeft = xLeft - colWidth
            columnIndex = columnIndex + 1
        end

        if columnIndex == 0 then return end

        writeToClipboard(lv.Selected.SubItems[columnIndex - 1])
    end

    -- -- Does this actually exist in CE lua's API?
    -- form.OnResize = function(sender)
    --     layoutBottom()
    -- end

    form:show()
end

return module
