#include "map_file.hpp"
#include "engine/tags/tag_group_id.hpp"
#include "structure_bsp.hpp"
#include "schema/reference.hpp"

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
        return (TagDataHeader*) fromRelative(cache->tagData);
    }

    void RawMapFile::normalize(Schema* schema) {
        Context context = {0};
        context.mapFile = this;
        context.schema = schema;
        
        if (isNormalized) return;
        isNormalized = true;

        for (uint32_t i = 0; i < getTagCount(); i++) {
            Tag* t = getTag(i);
            if (!t) continue;
            if (t->groupID != GroupId_BSP) continue;
            normalizeStructureBspTag(t, &context);
        }
    }

}
