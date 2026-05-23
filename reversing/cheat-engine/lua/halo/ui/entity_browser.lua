-- Entity Browser form for Cheat Engine.
-- Live-entity equivalent of tag_browser.lua, browsing runtime Halo 1 entities.

local module = {}

local entities   = reloadPackage("lua/halo/engine/entities")
local memoryView = reloadPackage("lua/functions/memory_view")

local PAGE_SIZE = 50

-- Column definitions, ordered left-to-right.
-- ListView mapping: COLUMNS[1] -> item.Caption, COLUMNS[n] -> item.SubItems[n-2]
module.COLUMNS = {
    { caption = "Handle",      width = 90  },
    { caption = "TagHandle",   width = 65  },
    { caption = "Category",    width = 90  },
    { caption = "Address",     width = 135 },
    { caption = "Health",      width = 60  },
    { caption = "Shield",      width = 60  },
    { caption = "Pos",         width = 185 },
    { caption = "TagPath",     width = 490 },
}

-- SubItems column indices (0-based) for named fields
local COL_SI_TYPE_ID  = 0
local COL_SI_CATEGORY = 1
local COL_SI_ADDRESS  = 2
local COL_SI_HEALTH   = 3
local COL_SI_SHIELD   = 4
local COL_SI_POS      = 5
local COL_SI_TAGPATH  = 6

local state = {
    form        = nil,
    results     = {},
    page        = 0,
    timer       = nil,
    autoRefresh = false,
}

local function pageCount()
    return math.max(1, math.ceil(#state.results / PAGE_SIZE))
end

local function clampPage()
    state.page = math.max(0, math.min(state.page, pageCount() - 1))
end

local function formatFloat(f)
    if not f then return "-" end
    return string.format("%.2f", f)
end

local function formatPos(pos)
    if not pos then return "-" end
    return string.format("%.1f, %.1f, %.1f", pos.x, pos.y, pos.z)
end

local function populateList(lv, pageLabel)
    lv.Items.Clear()
    clampPage()
    local base = state.page * PAGE_SIZE
    for i = 1, PAGE_SIZE do
        local ent = state.results[base + i]
        if not ent then break end
        local item = lv.Items.Add()
        item.Caption = string.format("%08X", ent.handle)
        item.SubItems.Add(string.format("%04X", ent.rawTagID))
        item.SubItems.Add(ent.categoryName)
        item.SubItems.Add(string.format("%X", ent.entityAddr))
        item.SubItems.Add(formatFloat(ent.health))
        item.SubItems.Add(formatFloat(ent.shield))
        item.SubItems.Add(formatPos(ent.pos))
        item.SubItems.Add(ent.tagPath or "")
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
    form.Caption  = "Entity Browser"
    form.Width    = 1060
    form.Height   = 640
    form.Position = poScreenCenter

    form.OnClose = function(sender)
        state.autoRefresh = false
        if state.timer then
            pcall(function() state.timer.Enabled = false end)
            state.timer = nil
        end
        state.form:destroy()
        state.form = nil
    end

    -- ------------------------------------------------------------------
    -- Row 1: category filter combo + text search input + buttons
    -- ------------------------------------------------------------------
    local combo = createComboBox(form)
    combo.Left  = 8
    combo.Top   = 8
    combo.Width = 120
    combo.Style = 2  -- csDropDownList: read-only dropdown
    for _, c in ipairs(entities.CATEGORIES) do
        combo.Items.Add(c.name)
    end
    combo.ItemIndex = 0

    local searchEdit = createEdit(form)
    searchEdit.Left  = combo.Left + combo.Width + 4
    searchEdit.Top   = 8
    searchEdit.Width = 560

    local searchBtn = createButton(form)
    searchBtn.Left    = searchEdit.Left + searchEdit.Width + 4
    searchBtn.Top     = 8
    searchBtn.Width   = 80
    searchBtn.Height  = 25
    searchBtn.Caption = "Search"

    local refreshBtn = createButton(form)
    refreshBtn.Left    = searchBtn.Left + searchBtn.Width + 4
    refreshBtn.Top     = 8
    refreshBtn.Width   = 80
    refreshBtn.Height  = 25
    refreshBtn.Caption = "Refresh"

    local autoChk = createCheckBox(form)
    autoChk.Left    = refreshBtn.Left + refreshBtn.Width + 8
    autoChk.Top     = 10
    autoChk.Width   = 115
    autoChk.Caption = "Auto-refresh"
    autoChk.Checked = false

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
    -- Row 4: pagination (anchored to bottom via layoutBottom)
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
    -- Filter state — set by doSearch, reused by doRefresh / auto-refresh
    -- ------------------------------------------------------------------
    local lastCatIndex  = 0
    local lastSearchTxt = ""

    local function makeFilterFn(catEntry, searchText)
        return function(ent)
            -- Category filter
            if catEntry and catEntry.id ~= -1 then
                if ent.category ~= catEntry.id then return false end
            end
            -- Text filter: handle hex, typeId hex, or tag path substring
            if searchText ~= "" then
                local handleHex = string.format("%08x", ent.handle)
                local typeHex   = string.format("%04x", ent.rawTagID)
                local path      = (ent.tagPath or ""):lower()
                local address   = string.format("%x", ent.entityAddr)
                if not path:find(searchText, 1, true)
                and not handleHex:find(searchText, 1, true)
                and not typeHex:find(searchText, 1, true)
                and not address:find(searchText, 1, true)
                then
                    return false
                end
            end
            return true
        end
    end

    local function applyFilter(catIndex, searchText, preservePage)
        local catEntry = entities.CATEGORIES[catIndex + 1]
        local filterFn = makeFilterFn(catEntry, searchText)

        if not entities.areEntitiesLoaded() then
            state.results = {}
            countLabel.Caption = "Entities not loaded"
            lv.Items.Clear()
            pageLabel.Caption = "Page 1 / 1"
            return
        end

        local savedPage = preservePage and state.page or 0
        state.results = entities.enumerateEntities(filterFn)
        state.page    = savedPage
        countLabel.Caption = string.format("%d entities", #state.results)
        populateList(lv, pageLabel)
    end

    local function doSearch()
        lastCatIndex  = combo.ItemIndex
        lastSearchTxt = searchEdit.Text:lower()
        applyFilter(lastCatIndex, lastSearchTxt, false)
    end

    local function doRefresh()
        applyFilter(lastCatIndex, lastSearchTxt, true)
    end

    -- ------------------------------------------------------------------
    -- Event handlers
    -- ------------------------------------------------------------------
    searchBtn.OnClick = function(sender)
        doSearch()
    end

    refreshBtn.OnClick = function(sender)
        doRefresh()
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

    -- Auto-refresh timer (1 second interval)
    local timer = createTimer(form)
    timer.Interval = 1000
    timer.Enabled  = false
    state.timer = timer

    timer.OnTimer = function(sender)
        if state.autoRefresh then
            pcall(doRefresh)
        end
    end

    autoChk.OnClick = function(sender)
        state.autoRefresh = autoChk.Checked
        timer.Enabled = state.autoRefresh
    end

    -- ------------------------------------------------------------------
    -- Double-click: open CE hex view at entity address
    -- ------------------------------------------------------------------
    lv.OnDblClick = function(sender)
        local item = lv.Selected
        if not item then return end
        local addr = tonumber(item.SubItems[COL_SI_ADDRESS], 16)
        if not addr or addr == 0 then return end
        local mv = memoryView.createHexView(addr)
        mv.Caption = "Entity: " .. item.Caption
        mv:show()
    end

    -- ------------------------------------------------------------------
    -- Context menu
    -- ------------------------------------------------------------------
    local popup = createPopupMenu(lv)

    local bpItem = createMenuItem(popup)
    bpItem.Caption = "Set access BP on entity"
    bpItem.OnClick = function(sender)
        local item = lv.Selected
        if not item then return end
        local addr = tonumber(item.SubItems[COL_SI_ADDRESS], 16)
        if not addr or addr == 0 then
            print("Entity Browser: selected entity has no valid address")
            return
        end
        debug_setBreakpoint(addr, 4, bptAccess)
        print(string.format("Entity Browser: access BP set at %X (entity: %s)", addr, item.Caption))
    end
    popup.Items.Add(bpItem)

    local copyAddrItem = createMenuItem(popup)
    copyAddrItem.Caption = "Copy address"
    copyAddrItem.OnClick = function(sender)
        local item = lv.Selected
        if not item then return end
        writeToClipboard(item.SubItems[COL_SI_ADDRESS])
    end
    popup.Items.Add(copyAddrItem)

    local copyHandleItem = createMenuItem(popup)
    copyHandleItem.Caption = "Copy handle"
    copyHandleItem.OnClick = function(sender)
        local item = lv.Selected
        if not item then return end
        writeToClipboard(item.Caption)
    end
    popup.Items.Add(copyHandleItem)

    local copyTagHandleItem = createMenuItem(popup)
    copyTagHandleItem.Caption = "Copy tag handle"
    copyTagHandleItem.OnClick = function(sender)
        local item = lv.Selected
        if not item then return end
        writeToClipboard(item.SubItems[COL_SI_TYPE_ID])
    end
    popup.Items.Add(copyTagHandleItem)

    lv.PopupMenu = popup

    form:show()
end

return module
