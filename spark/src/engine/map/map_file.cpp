#include "map_file.hpp"

namespace Engine::Map {

    CacheHeader* MapFile::getCacheHeader() {
        return (CacheHeader*)getPointerBase(PointerBase_Map);
    }

    uint32_t MapFile::getTagCount() {
        TagDataHeader* h = getTagDataHeader();
        if (!h) return 0;
        return h ? h->tagCount : 0;
    }

    Tag* MapFile::getTag(uint32_t handle) {
        uint32_t i = handle & 0xFFFF;
        TagDataHeader* tagHeader = this->getTagDataHeader();
        if (!tagHeader) return nullptr;
        if (i >= tagHeader->tagCount) return nullptr;
        void* base = fromRelative(tagHeader->tagArray);
        return ((Tag*)base) + i;
    }

    char* MapFile::getTagPath(Tag* t) {
        if (!t) return nullptr;
        return (char*) fromRelative(t->path);
    }

    void* MapFile::getTagData(Tag* t) {
        if (!t) return nullptr;
        return fromRelative(t->data);
    }

    uint32_t MapFile::getVertexDataSize() {
        TagDataHeader* h = getTagDataHeader();
        return h ? h->vertexDataSize : 0;
    }

    size_t MapFile::guessTagDataSize(uint32_t tagHandle) {
        uint32_t nextTagHandle = tagHandle + 1;
        Tag* currentTag = getTag(tagHandle);
        Tag* nextTag = getTag(nextTagHandle);
        if (!currentTag || !nextTag) return 0;
        void* currentData = getTagData(currentTag);
        void* nextData = getTagData(nextTag);
        return (uintptr_t)nextData - (uintptr_t)currentData;
    }

}
