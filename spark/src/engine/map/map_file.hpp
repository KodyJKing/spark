#pragma once

#include <cassert>

#include "tag_data_types.hpp"
#include "memory/Memory.hpp"

#define DEBUG_MAP_FILE

#ifdef DEBUG_MAP_FILE
#define ASSERT(x, msg) assert((x) && msg)
#else
#define ASSERT(x, msg)
#endif

namespace Engine::Map {
    
    // Abstract map file class
    class MapFile {
        public:

        template<PointerBase B, typename T = void>
        T* fromRelative(Pointer<B, T> p) {
            return (T*)((char*)getPointerBase(B) + p.offset);
        }

        template<PointerBase B, typename T = void>
        Pointer<B, T> toRelative(void* ptr) {
            int64_t offset = (char*)ptr - (char*)getPointerBase(B);
            uint32_t offset32 = static_cast<uint32_t>(offset);
            ASSERT(offset32 == offset, "Pointer offset does not fit in 32 bits");
            return { offset32 };
        }

        template<typename T>
        T* getBlockElement(BlockPointerTyped<T>& block, size_t i) {
            if (i >= block.count) return nullptr;
            T* elements = (T*) fromRelative(block.data);
            return (T*) &elements[i];
        }

        CacheHeader* getCacheHeader();
        uint32_t getTagCount();
        Tag* getTag(uint32_t handle);
        char* getTagPath(Tag* t);
        void* getTagData(Tag* t);
        uint32_t getVertexDataSize();
        size_t guessTagDataSize(uint32_t tagHandle);

        // Gets base ptr for a section of the map file
        virtual void* getPointerBase(PointerBase b) = 0;
        virtual TagDataHeader* getTagDataHeader() = 0;
        private:
    };

    class RawMapFile : public MapFile {
        public:

        RawMapFile() = default;
        RawMapFile(CacheHeader* cache) : cache(cache) {}

        void* getPointerBase(PointerBase b) override;
        TagDataHeader* getTagDataHeader() override;

        private:
        CacheHeader* cache = nullptr;
    };

    class RuntimeMapFile : public MapFile {
        public:

        void* getPointerBase(PointerBase b) override;
        TagDataHeader* getTagDataHeader() override;

        // Copies tag from another map.
        Tag* copyTag(MapFile* sourceMap, uint32_t sourceTagHandle);
        
        void* allocate(size_t size);
        void free(void *ptr);
        Tag *allocateTag();

    private:
    };

}
