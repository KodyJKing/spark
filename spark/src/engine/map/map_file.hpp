#pragma once

#include "tag_data_types.hpp"
#include "memory/Memory.hpp"

namespace Engine::Map {
    
    // Abstract map file class
    class MapFile {
        public:
        template<PointerBase B>
        void* fromRelative(Pointer<B> p) {
            return (char*)getPointerBase(B) + p.offset;
        }

        template<PointerBase B>
        Pointer<B> toRelative(void* ptr) {
            return { (char*)ptr - (char*)getPointerBase(B) };
        }

        inline CacheHeader* getCacheHeader() {
            return (CacheHeader*)getPointerBase(PointerBase_Map);
        }

        inline uint32_t getTagCount() {
            TagDataHeader* h = getTagDataHeader();
            if (!h) return 0;
            return h ? h->tagCount : 0;
        }

        inline Tag* getTag(uint32_t handle) {
            uint32_t i = handle & 0xFFFF;
            TagDataHeader* tagHeader = this->getTagDataHeader();
            if (!tagHeader) return nullptr;
            if (i >= tagHeader->tagCount) return nullptr;
            void* base = fromRelative(tagHeader->tagArray);
            return ((Tag*)base) + i;
        }

        inline char* getTagPath(Tag* t) {
            if (!t) return nullptr;
            return (char*) fromRelative(t->path);
        }

        inline void* getTagData(Tag* t) {
            if (!t) return nullptr;
            return fromRelative(t->data);
        }

        inline uint32_t getVertexDataSize() {
            TagDataHeader* h = getTagDataHeader();
            return h ? h->vertexDataSize : 0;
        }

        inline size_t guessTagDataSize(uint32_t tagHandle) {
            uint32_t nextTagHandle = tagHandle + 1;
            Tag* currentTag = getTag(tagHandle);
            Tag* nextTag = getTag(nextTagHandle);
            if (!currentTag || !nextTag) return 0;
            void* currentData = getTagData(currentTag);
            void* nextData = getTagData(nextTag);
            return (uintptr_t)nextData - (uintptr_t)currentData;
        }

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

        // // Copies tag from another map.
        // // Handles allocation of tag, copying 
        // uint32_t copyTag(MapFile* sourceMap, uint32_t sourceTagHandle);
        
        private:

        //  // Copy tag data to a destination buffer. Fix any pointers or tag references.
        //  bool copyTagData(MapFile* sourceMap, uint32_t sourceTagHandle, void* dest, size_t size);

        //  uint32_t allocateTag();
    };

}
