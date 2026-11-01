#include "map_file.hpp"
#include "engine/tags/tag_group_id.hpp"
#include "structure_bsp.hpp"

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

    void* MapFile::getTagDataNaive(Tag* t) {
        if (!t) return nullptr;
        return fromRelative(t->data);
    }

    void* MapFile::getTagData(Tag* t) {
        if (!t) return nullptr;
        if (!t->data.offset && t->groupID == GroupId_BSP) {
            StructureBspReference* ref = getStructureBspReference(this, t);
            if (!ref) return nullptr;
            char* header = (char*)fromRelative(ref->structureBspHeader);
            return header + sizeof(StructureBspHeader);
        } else {
            return fromRelative(t->data);
        }
    }

    uint32_t MapFile::getVertexDataSize() {
        TagDataHeader* h = getTagDataHeader();
        return h ? h->vertexDataSize : 0;
    }

    size_t MapFile::guessTagDataSize(uint32_t tagHandle) {
        Tag* currentTag = getTag(tagHandle);
        if (!currentTag) return 0;
        
        if (currentTag->groupID == GroupId_BSP) {
            StructureBspReference* ref = getStructureBspReference(this, currentTag);
            if (!ref) return 0;
            return ref->size;
        }
        
        uint32_t nextTagHandle = tagHandle + 1;
        Tag* nextTag = getTag(nextTagHandle);
        if (!nextTag) return 0;
        void* currentData = getTagData(currentTag);
        void* nextData = getTagData(nextTag);
        return (uintptr_t)nextData - (uintptr_t)currentData;
    }

    Tag* MapFile::findTag(uint32_t groupId, const char* path) {
        size_t count = getTagCount();
        for (size_t i = 0; i < count; i++) {
            Tag* tag = getTag(i);
            if (!tag || tag->groupID != groupId) continue;
            const char* tagPath = getTagPath(tag);
            if (strncmp(path, tagPath, 1024) == 0)
                return tag;
        }
        return nullptr;
    }

}
