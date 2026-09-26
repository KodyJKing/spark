#pragma once

#include "tag_data_types.hpp"

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

        inline TagDataHeader* getTagDataHeader() {
            return (TagDataHeader*)getPointerBase(PointerBase_Tags);
        }

        inline Tag* getTag(uint32_t i) {
            TagDataHeader* tagHeader = this->getTagDataHeader();
            void* base = (char*) tagHeader + tagHeader->tagArray.offset;
            return ((Tag*)base) + i;
        }

        inline uint32_t getVertexDataSize() {
            TagDataHeader* h = getTagDataHeader();
            return h ? h->vertexDataSize : 0;
        }

        // Gets base ptr for a section of the map file
        virtual void* getPointerBase(PointerBase b) = 0;
        
        private:
    };

    class RawMapFile : public MapFile {
        public:

        RawMapFile() = default;
        RawMapFile(CacheHeader* cache) : cache(cache) {}

        void* getPointerBase(PointerBase b) override;

        private:
        CacheHeader* cache = nullptr;
    };

    class RuntimeMapFile : public MapFile {
        public:

        
        void* getPointerBase(PointerBase b) override;
        // // Copies tag from another map.
        // // Handles allocation of tag, copying 
        // uint32_t copyTag(MapFile* sourceMap, uint32_t sourceTagHandle);
        
        private:

        //  // Copy tag data to a destination buffer. Fix any pointers or tag references.
        //  bool copyTagData(MapFile* sourceMap, uint32_t sourceTagHandle, void* dest, size_t size);

        //  uint32_t allocateTag();
    };

}
