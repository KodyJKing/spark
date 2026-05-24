-- chunk_index.lua
-- Generic address-space bucket index for fast O(1)-bucket lookups.
-- Neutral with respect to item shape; callers supply address selectors.

local CHUNK_SIZE = 0x4000

local ChunkIndex = {}
ChunkIndex.__index = ChunkIndex

-- Build an index from a list of items.
-- addressFunc(item) returns the address used to assign the item to its bucket.
function ChunkIndex.new(items, addressFunc)
    local self = setmetatable({ _buckets = {} }, ChunkIndex)
    for _, item in ipairs(items) do
        local key = math.floor(addressFunc(item) / CHUNK_SIZE)
        if self._buckets[key] == nil then self._buckets[key] = {} end
        table.insert(self._buckets[key], item)
    end
    return self
end

-- Returns the first item in the bucket where item.address == addr, or nil.
function ChunkIndex:lookupExact(addr)
    local bucket = self._buckets[math.floor(addr / CHUNK_SIZE)]
    if bucket == nil then return nil end
    for _, item in ipairs(bucket) do
        if item.address == addr then return item end
    end
    return nil
end

-- Returns the first item in the bucket where addr >= item.start and addr <= item.stop, or nil.
function ChunkIndex:lookupRange(addr)
    local bucket = self._buckets[math.floor(addr / CHUNK_SIZE)]
    if bucket == nil then return nil end
    for _, item in ipairs(bucket) do
        if addr >= item.start and addr <= item.stop then return item end
    end
    return nil
end

return ChunkIndex
