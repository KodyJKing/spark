#include "map_file.hpp"

namespace Engine::Map {

    void* RawMapFile::getPointerBase(PointerBase b) {
        switch(b) {
            case PointerBase_Map: return cache;
            case PointerBase_Tags: return (char*)cache + cache->tagData.offset - 0x50000000;
            case PointerBase_Vertices: return (char*)cache + getTagDataHeader()->modelData.offset;
            case PointerBase_Indices: return (char*)getPointerBase(PointerBase_Vertices) + getVertexDataSize();
            default: return nullptr;
        }
    }

    TagDataHeader* RawMapFile::getTagDataHeader() {
        CacheHeader* c = cache;
        return (TagDataHeader*) fromRelative(cache->tagData);
    }

}
