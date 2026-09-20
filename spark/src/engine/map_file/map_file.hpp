#pragma once

// Standalone port of reversing/imhex/H1_Cache.hexpat.
// Parses a Halo 1 .map cache file directly from a file buffer. Intentionally
// does not share types with Engine::Tag or any other live-process structures.

#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Engine::MapFile {

    struct Vec3 { float x, y, z; };
    struct Vec4 { float x, y, z, w; };

#pragma pack(push, 1)

    // A block of `count` T's starting at `offset`. Resolve with MapFile::resolveBlock.
    template <typename T>
    struct BlockPointer {
        uint32_t count;
        uint32_t offset;
        uint32_t bullshit; // Unknown purpose.
    };

    namespace TagData {

        // Vertex size in bytes, indexed by VertexDataPointer::type.
        inline constexpr uint16_t kVertexSizes[20] = {
            0x38, 0x20, 0x14, 0x08, 0x44, 0x20, 0x18, 0x24, 0x18, 0x10,
            0x14, 0x14, 0x20, 0x08, 0x20, 0x20, 0x24, 0x1C, 0x00, 0x00
        };

        struct VertexDataPointer {
            uint16_t type;
            uint16_t unused0;
            uint32_t count;
            char pad0[8]; // Present in loaded tag data despite being absent from the on-disk layout docs.
            uint32_t offset; // Use MapFile::translateVertexDataPointer
        };

        struct EffectLocation {
            char name[32];
        };

        struct Effect {
            char pad0[0x28];
            BlockPointer<EffectLocation> locations;
        };

        // Fields below are typed overlays at fixed byte offsets within `raw`, matching the
        // hexpat "@ THIS+offset" annotations rather than a sequential field layout.
        struct ModelGeometryPart {
            char raw[0x84];
            Vec3* centroid() { return reinterpret_cast<Vec3*>(raw + 0x14); }
            VertexDataPointer* vertexData() { return reinterpret_cast<VertexDataPointer*>(raw + 0x54); }
        };

        struct ModelGeometry {
            char raw[0x30];
            BlockPointer<ModelGeometryPart>* parts() { return reinterpret_cast<BlockPointer<ModelGeometryPart>*>(raw + 0x24); }
        };

        struct Model {
            char raw[0xDC];
            // geometries overlaps past the end of `raw`; offset is relative to the struct start.
            BlockPointer<ModelGeometry>* geometries() { return reinterpret_cast<BlockPointer<ModelGeometry>*>(reinterpret_cast<char*>(this) + 0xD0); }
        };

        struct BitmapGroupsSequence {
            char raw[0x40];
        };

        struct BitmapData {
            uint32_t bitmapClass;
            uint16_t width, height, depth;
            uint16_t type, format, flags;
            int16_t regX, regY;
            uint16_t minMapCount;
            uint16_t unused0;
            uint32_t dataOffset;
            uint32_t dataSize;
            uint32_t thisBitmapHandle;
            uint32_t texCacheHandle;
            uint32_t bufferSourceIndex;
        };

        struct Bitmap {
            char raw[0x6C];
            BlockPointer<BitmapGroupsSequence>* groupSequences() { return reinterpret_cast<BlockPointer<BitmapGroupsSequence>*>(reinterpret_cast<char*>(this) + 0x54); }
            BlockPointer<BitmapData>* bitmapData() { return reinterpret_cast<BlockPointer<BitmapData>*>(reinterpret_cast<char*>(this) + 0x60); }
        };

    }

    struct Tag {
        char groupId[4];
        char parentGroupId[4];
        char grandparentGroupId[4];
        uint32_t tagID;
        uint32_t pathPtr;
        uint32_t dataPtr;
        char pad0[8];
    };

    struct TagDataHeader {
        uint32_t tagArrayPtr;
        uint32_t checksum;
        uint32_t scenarioId;
        uint32_t tagCount;
        uint32_t modelPartCount;
        uint32_t modelDataOffset;
        uint32_t modelPartCount2;
        uint32_t vertexDataSize;
        uint32_t modelDataSize;
        char magic[4];
    };

    struct CacheHeader {
        char magic[4];
        uint32_t cacheVersion;
        uint32_t fileSize;
        uint32_t paddingLength;
        uint32_t tagDataOffset;
        uint32_t tagDataSize;
        char pad0[8];
        char scenarioName[32];
        char buildVersion[32];
        uint16_t scenarioType;
        char pad1[2];
        uint32_t checksum;
    };

#pragma pack(pop)

    // Loads a Halo 1 .map cache file from disk and resolves the in-file pointer
    // scheme used by the tag data blob (see H1_Cache.hexpat).
    class MapFile {
    public:
        bool loadFromFile(const std::string& path);

        CacheHeader* getCacheHeader();
        TagDataHeader* getTagDataHeader();

        uint32_t getTagCount();
        Tag* getTag(uint32_t index);
        const char* getTagPath(const Tag& tag);
        void* getTagData(const Tag& tag);

        TagData::Effect* getEffectData(const Tag& tag) { return getTypedTagData<TagData::Effect>(tag, "effe"); }
        TagData::Model* getModelData(const Tag& tag) { return getTypedTagData<TagData::Model>(tag, "2dom"); }
        TagData::Bitmap* getBitmapData(const Tag& tag) { return getTypedTagData<TagData::Bitmap>(tag, "mtib"); }

        std::pair<uint8_t*, size_t> getVertexData(const TagData::VertexDataPointer& vertexData);

        uint8_t* translatePointer(uint32_t pointer);
        uint8_t* translateVertexDataPointer(uint32_t pointer);

        void* getTextureDataPointer(const TagData::BitmapData& bitmapData) { 
            if (!isValidRange(bitmapData.dataOffset, bitmapData.dataSize)) return nullptr;
            return m_buffer.data() + bitmapData.dataOffset;
        }

        template <typename T>
        T* resolveBlock(const BlockPointer<T>& block) {
            uint32_t offset = computePointerOffset(block.offset);
            uint64_t size = static_cast<uint64_t>(sizeof(T)) * block.count;
            if (!isValidRange(offset, size)) return nullptr;
            return reinterpret_cast<T*>(m_buffer.data() + offset);
        }

    private:
        template <typename T>
        T* getTypedTagData(const Tag& tag, const char* fourCc) {
            if (!tagGroupIs(tag, fourCc)) return nullptr;
            uint32_t offset = computePointerOffset(tag.dataPtr);
            if (!isValidRange(offset, sizeof(T))) return nullptr;
            return reinterpret_cast<T*>(m_buffer.data() + offset);
        }

        bool tagGroupIs(const Tag& tag, const char* fourCc) const;
        uint32_t computePointerOffset(uint32_t pointer) const;
        bool isValidRange(uint64_t offset, uint64_t size) const;

        std::vector<uint8_t> m_buffer;
        uint32_t m_tagDataHeaderOffset = 0;
    };

}
