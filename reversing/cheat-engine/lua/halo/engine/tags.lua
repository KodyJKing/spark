-- Engine API for reading Halo 1 tag data from memory.
-- Mirrors functionality from smc64/src/haloce/halo1/tag.cpp and map.cpp

local module = {}

-- Tag struct layout (32 bytes / 0x20 per entry)
-- Mirrors Halo1::Tag in tag.hpp
local STRUCT_TAG_SIZE = 0x20
local STRUCT_OFF_GROUP_ID              = 0x00
local STRUCT_OFF_PARENT_GROUP_ID       = 0x04
local STRUCT_OFF_GRANDPARENT_GROUP_ID  = 0x08
local STRUCT_OFF_TAG_ID                = 0x0C
local STRUCT_OFF_RESOURCE_PATH_ADDRESS = 0x10
local STRUCT_OFF_DATA_ADDRESS          = 0x14

-- Pointer to tag array base; dereference to get Tag* array.
-- Mirrors: Tag* tagArray = *(Tag**)(dllBase() + 0x1C34FB0) in tag.cpp
local TAG_ARRAY_PTR = "halo1.dll+1C34FB0"

-- Map address translation.
-- Mirrors Halo1::translateMapAddress in map.cpp
-- See also: lua/functions/halo1.lua (relocatedMapBase / mapBase)
local function mapOffset()
    local ok1, relocated = pcall(readQword, "halo1.dll+2D9CE10")
    local ok2, base      = pcall(readQword, "halo1.dll+2EA3410")
    if not ok1 or not ok2 then return 0 end
    return relocated - base
end

local function fromMapAddress(addr)
    if not addr or addr == 0 then return nil end
    return addr + mapOffset()
end

-- Mirrors Strings::fourccToString
local function fourccToString(v)
    if not v or v == 0 then return "    " end
    local function safeChar(byte)
        if byte >= 0x20 and byte <= 0x7E then return string.char(byte) end
        return "."
    end
    return safeChar((v >> 24) & 0xFF)
        .. safeChar((v >> 16) & 0xFF)
        .. safeChar((v >>  8) & 0xFF)
        .. safeChar( v        & 0xFF)
end
module.fourccToString = fourccToString

-- Mirrors Strings::stringToFourcc
local function stringToFourcc(s)
    if not s or #s < 4 then return 0 end
    return (string.byte(s, 1) << 24)
         | (string.byte(s, 2) << 16)
         | (string.byte(s, 3) <<  8)
         |  string.byte(s, 4)
end
module.stringToFourcc = stringToFourcc

-- Mirrors Halo1::validTagPath
local function validTagPath(path)
    if not path or #path < 3 then return false end
    local backslashCount = 0
    for i = 1, math.min(#path, 512) do
        local c = path:sub(i, i)
        if c == "\\" then
            backslashCount = backslashCount + 1
        elseif not c:match("[a-zA-Z0-9_ %.%-]") then
            return false
        end
    end
    return backslashCount > 0
end

-- Matches the ids[] array in TagBrowser.cpp
module.GROUP_IDS = {
    { name = "All",             fourcc = 0                          },
    { name = "Weapon",          fourcc = stringToFourcc("weap")     },
    { name = "Projectile",      fourcc = stringToFourcc("proj")     },
    { name = "Damage",          fourcc = stringToFourcc("jpt!")     },
    { name = "Vehicle",         fourcc = stringToFourcc("vehi")     },
    { name = "Biped",           fourcc = stringToFourcc("bipd")     },
    { name = "Scenery",         fourcc = stringToFourcc("scen")     },
    { name = "Device",          fourcc = stringToFourcc("devi")     },
    { name = "Effect",          fourcc = stringToFourcc("effe")     },
    { name = "Contrail",        fourcc = stringToFourcc("cont")     },
    { name = "Particle",        fourcc = stringToFourcc("part")     },
    { name = "Sound",           fourcc = stringToFourcc("snd!")     },
    { name = "Animation",       fourcc = stringToFourcc("antr")     },
    { name = "Actor",           fourcc = stringToFourcc("actr")     },
    { name = "Actor Variant",   fourcc = stringToFourcc("actv")     },
    { name = "Bitmap",          fourcc = stringToFourcc("bitm")     },
    { name = "Shader",          fourcc = stringToFourcc("shdr")     },
    { name = "Light",           fourcc = stringToFourcc("ligh")     },
    { name = "BSP",             fourcc = stringToFourcc("sbsp")     },
    { name = "Collision Model", fourcc = stringToFourcc("coll")     },
}

function module.tagArrayBase()
    local ok, result = pcall(readQword, TAG_ARRAY_PTR)
    if ok then return result end
    return nil
end

-- Internal: read one tag from a pre-fetched array base.
local function readTagFromBase(base, index)
    local addr = base + index * STRUCT_TAG_SIZE

    local ok, result = pcall(function()
        local groupID             = readInteger(addr + STRUCT_OFF_GROUP_ID)              & 0xFFFFFFFF
        local parentGroupID       = readInteger(addr + STRUCT_OFF_PARENT_GROUP_ID)       & 0xFFFFFFFF
        local grandparentGroupID  = readInteger(addr + STRUCT_OFF_GRANDPARENT_GROUP_ID)  & 0xFFFFFFFF
        local tagID               = readInteger(addr + STRUCT_OFF_TAG_ID)                & 0xFFFFFFFF
        local resourcePathAddress = readInteger(addr + STRUCT_OFF_RESOURCE_PATH_ADDRESS) & 0xFFFFFFFF
        local dataAddress         = readInteger(addr + STRUCT_OFF_DATA_ADDRESS)          & 0xFFFFFFFF
        local pDataAddress        = addr + STRUCT_OFF_DATA_ADDRESS

        local path = nil
        local absPathAddr = fromMapAddress(resourcePathAddress)
        if absPathAddr then
            local pathOk, pathResult = pcall(readString, absPathAddr, 512)
            if pathOk and pathResult and pathResult ~= "" then path = pathResult end
        end

        return {
            index               = index,
            groupID             = groupID,
            parentGroupID       = parentGroupID,
            grandparentGroupID  = grandparentGroupID,
            tagID               = tagID,
            resourcePathAddress = resourcePathAddress,
            dataAddress         = dataAddress,
            absDataAddress      = fromMapAddress(dataAddress),
            pDataAddress       = pDataAddress,
            path                = path,
            groupIDStr          = fourccToString(groupID),
        }
    end)

    return ok and result or nil
end

-- Public single-tag accessor. Mirrors Halo1::getTag.
function module.readTag(index)
    local base = module.tagArrayBase()
    if not base or base == 0 then return nil end
    return readTagFromBase(base, index)
end

-- Mirrors Halo1::tagExists
function module.tagExists(tag)
    if not tag then return false end
    return validTagPath(tag.path)
end

-- Iterate all tags, returning a filtered list.
-- Mirrors the iteration loop in Halo1::findTag / TagBrowser search loop.
-- filterFn(tag) -> bool; pass nil to return everything.
function module.enumerateTags(filterFn)
    local base = module.tagArrayBase()
    if not base or base == 0 then return {} end

    local results = {}
    for i = 0, 0xFFFF do
        local tag = readTagFromBase(base, i)
        if not module.tagExists(tag) then break end
        if not filterFn or filterFn(tag) then
            table.insert(results, tag)
        end
    end
    return results
end

return module
