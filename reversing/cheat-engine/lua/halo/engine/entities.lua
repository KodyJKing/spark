-- Engine API for reading Halo 1 entity data from memory.
-- Mirrors functionality from smc64/src/haloce/halo1/entity/entity_list.cpp

local module = {}

local tags = reloadPackage("lua/halo/engine/tags")

-- -------------------------------------------------------------------------
-- Memory addresses — mirrors entity_list.cpp
-- -------------------------------------------------------------------------
local ENTITY_LIST_PTR       = "halo1.dll+1C42248"  -- EntityList**
local ENTITY_ARRAY_BASE_PTR = "halo1.dll+2D9CDF8"  -- uintptr_t*

-- -------------------------------------------------------------------------
-- EntityList struct offsets — mirrors Halo1::EntityList in types.hpp
-- -------------------------------------------------------------------------
local ENTITYLIST_OFF_CAPACITY        = 0x20  -- uint16_t
local ENTITYLIST_OFF_COUNT           = 0x2E  -- uint16_t
local ENTITYLIST_OFF_ENTITYLIST_OFF  = 0x34  -- int32_t

-- -------------------------------------------------------------------------
-- EntityRecord struct — 12 bytes per slot — mirrors Halo1::EntityRecord
-- -------------------------------------------------------------------------
local ENTITYRECORD_SIZE             = 0x0C
local ENTITYRECORD_OFF_ID           = 0x00  -- uint16_t
-- +0x02 unknown_1 (uint16), +0x04 unknown_2 (uint16)
local ENTITYRECORD_OFF_TYPE_ID      = 0x06  -- uint16_t
local ENTITYRECORD_OFF_ARRAY_OFFSET = 0x08  -- int32_t (signed; -1 = slot empty)

-- -------------------------------------------------------------------------
-- Entity struct offsets — mirrors Halo1::Entity in types.hpp
-- -------------------------------------------------------------------------
local ENTITY_OFF_TAG_ID   = 0x00  -- uint32_t
local ENTITY_OFF_AGE_MS   = 0x14  -- uint32_t
local ENTITY_OFF_POS_X    = 0x18  -- float
local ENTITY_OFF_POS_Y    = 0x1C  -- float
local ENTITY_OFF_POS_Z    = 0x20  -- float
local ENTITY_OFF_CATEGORY = 0x70  -- uint16_t
local ENTITY_OFF_CTRL_HDL = 0x7C  -- uint32_t (controllerHandle)
local ENTITY_OFF_HEALTH   = 0x9C  -- float
local ENTITY_OFF_SHIELD   = 0xA0  -- float
local ENTITY_OFF_VEH_HDL  = 0xD4  -- uint32_t (vehicleHandle)
local ENTITY_OFF_CHILD_HDL  = 0xD8  -- uint32_t
local ENTITY_OFF_PARENT_HDL = 0xDC  -- uint32_t

-- Mirrors: entityAddress = getEntityArrayBase() + 0x34 + record.entityArrayOffset
local ENTITY_ARRAY_ELEM_OFFSET = 0x34

-- -------------------------------------------------------------------------
-- EntityCategory enum — mirrors Halo1::EntityCategory in types.hpp
-- -------------------------------------------------------------------------
module.CATEGORIES = {
    { name = "All",          id = -1 },
    { name = "Biped",        id = 0  },
    { name = "Vehicle",      id = 1  },
    { name = "Weapon",       id = 2  },
    { name = "Equipment",    id = 3  },
    { name = "Garbage",      id = 4  },
    { name = "Projectile",   id = 5  },
    { name = "Scenery",      id = 6  },
    { name = "Machine",      id = 7  },
    { name = "Control",      id = 8  },
    { name = "LightFixture", id = 9  },
    { name = "Placeholder",  id = 10 },
    { name = "SoundScenery", id = 11 },
}

local CATEGORY_NAMES = {}
for _, c in ipairs(module.CATEGORIES) do
    CATEGORY_NAMES[c.id] = c.name
end

-- -------------------------------------------------------------------------
-- Pointer helpers
-- -------------------------------------------------------------------------
local function entityListPtr()
    local ok, v = pcall(readQword, ENTITY_LIST_PTR)
    return ok and v or nil
end

local function entityArrayBase()
    local ok, v = pcall(readQword, ENTITY_ARRAY_BASE_PTR)
    return ok and v or nil
end

-- Mirrors areEntitiesLoaded() in entity_list.cpp
function module.areEntitiesLoaded()
    local pList  = entityListPtr()
    local pArray = entityArrayBase()
    return pList  ~= nil and pList  ~= 0
       and pArray ~= nil and pArray ~= 0
end

-- -------------------------------------------------------------------------
-- Read one EntityRecord at slot i from a pre-fetched list base.
-- Mirrors getEntityRecord(pEntityList, i) in entity_list.cpp
-- -------------------------------------------------------------------------
local function readRecord(pEntityList, listOffset, i)
    local addr = pEntityList + listOffset + i * ENTITYRECORD_SIZE
    local ok, result = pcall(function()
        local id          = readSmallInteger(addr + ENTITYRECORD_OFF_ID)           & 0xFFFF
        local typeId      = readSmallInteger(addr + ENTITYRECORD_OFF_TYPE_ID)      & 0xFFFF
        local arrayOffset = readInteger     (addr + ENTITYRECORD_OFF_ARRAY_OFFSET) -- signed int32
        return { id = id, typeId = typeId, arrayOffset = arrayOffset }
    end)
    return ok and result or nil
end

-- -------------------------------------------------------------------------
-- Read key fields from one Entity at entityAddr.
-- tagPathCache is a per-enumeration table[tagIndex] → path string, to avoid
-- re-reading the same tag for every entity of the same type.
-- -------------------------------------------------------------------------
local function readEntityFields(entityAddr, record, index, tagPathCache)
    local ok, result = pcall(function()
        local rawTagID = readInteger(entityAddr + ENTITY_OFF_TAG_ID)   & 0xFFFFFFFF
        local category = readSmallInteger(entityAddr + ENTITY_OFF_CATEGORY) & 0xFFFF
        local health   = readFloat(entityAddr + ENTITY_OFF_HEALTH)
        local shield   = readFloat(entityAddr + ENTITY_OFF_SHIELD)
        local posX     = readFloat(entityAddr + ENTITY_OFF_POS_X)
        local posY     = readFloat(entityAddr + ENTITY_OFF_POS_Y)
        local posZ     = readFloat(entityAddr + ENTITY_OFF_POS_Z)

        -- Tag path: look up by the lower 16 bits of the entity's tagID.
        -- Same index scheme as tag handles (tagID & 0xFFFF = tag array index).
        local tagIndex = rawTagID & 0xFFFF
        local tagPath  = tagPathCache[tagIndex]
        if tagPath == nil then
            local tag  = tags.readTag(tagIndex)
            tagPath    = (tag and tag.path) or ""
            tagPathCache[tagIndex] = tagPath
        end

        -- Entity handle = (record.id << 16) | index
        -- Mirrors indexToEntityHandle() in entity_list.cpp
        local handle = ((record.id & 0xFFFF) << 16) | (index & 0xFFFF)

        return {
            index        = index,
            handle       = handle,
            typeId       = record.typeId,
            entityAddr   = entityAddr,
            rawTagID     = rawTagID,
            category     = category,
            categoryName = CATEGORY_NAMES[category] or ("Cat" .. tostring(category)),
            health       = health,
            shield       = shield,
            pos          = { x = posX, y = posY, z = posZ },
            tagPath      = tagPath,
        }
    end)
    return ok and result or nil
end

-- -------------------------------------------------------------------------
-- Enumerate all live entities, optionally filtered.
-- filterFn(entity) -> bool; pass nil to return everything.
-- Mirrors foreachEntityRecord() in entity_list.cpp
-- -------------------------------------------------------------------------
function module.enumerateEntities(filterFn)
    local pList  = entityListPtr()
    local pArray = entityArrayBase()
    if not pList or pList == 0 or not pArray or pArray == 0 then
        return {}
    end

    local okCap, capacity = pcall(readSmallInteger, pList + ENTITYLIST_OFF_CAPACITY)
    if not okCap or not capacity then return {} end
    capacity = capacity & 0xFFFF
    if capacity == 0 then return {} end

    local okOff, listOffset = pcall(readInteger, pList + ENTITYLIST_OFF_ENTITYLIST_OFF)
    if not okOff or not listOffset then return {} end

    local tagPathCache = {}
    local results = {}

    for i = 0, capacity - 1 do
        local rec = readRecord(pList, listOffset, i)
        -- arrayOffset == -1 (or 0xFFFFFFFF unsigned) means the slot is empty
        if rec and rec.arrayOffset ~= -1 and rec.arrayOffset ~= 0xFFFFFFFF then
            local entityAddr = pArray + ENTITY_ARRAY_ELEM_OFFSET + rec.arrayOffset
            local ent = readEntityFields(entityAddr, rec, i, tagPathCache)
            if ent and (not filterFn or filterFn(ent)) then
                table.insert(results, ent)
            end
        end
    end

    return results
end

-- -------------------------------------------------------------------------
-- Look up a single live entity by its 32-bit handle.
-- handle layout: upper 16 bits = record id (generation), lower 16 bits = slot index.
-- Returns the entity table on success, nil if the slot is empty or the id
-- does not match (i.e. the handle is stale or invalid).
-- -------------------------------------------------------------------------
function module.getEntityByHandle(handle)
    if not handle or handle == 0 or handle == 0xFFFFFFFF then return nil end

    local slotIndex = handle & 0xFFFF
    local slotId    = (handle >> 16) & 0xFFFF

    local pList  = entityListPtr()
    local pArray = entityArrayBase()
    if not pList or pList == 0 or not pArray or pArray == 0 then return nil end

    local okOff, listOffset = pcall(readInteger, pList + ENTITYLIST_OFF_ENTITYLIST_OFF)
    if not okOff or not listOffset then return nil end

    local rec = readRecord(pList, listOffset, slotIndex)
    if not rec then return nil end
    if rec.arrayOffset == -1 or rec.arrayOffset == 0xFFFFFFFF then return nil end
    -- Validate generation id so stale handles are rejected.
    if rec.id ~= slotId then return nil end

    local entityAddr = pArray + ENTITY_ARRAY_ELEM_OFFSET + rec.arrayOffset
    return readEntityFields(entityAddr, rec, slotIndex, {})
end

return module
