#include "copy_tag.hpp"
#include "get_tag_data_size.hpp"

namespace Engine::Map {

    Tag* copyTag(
        Schema* schema,
        MapFile* fromMap,
        RuntimeMapFile* toMap,
        uint32_t fromTagHandle
    ) {
        Tag* fromTag = fromMap->getTag(fromTagHandle);
        if (!fromTag) return nullptr;
        char* fromTagPath = fromMap->getTagPath(fromTag);
        if (!fromTagPath) return nullptr;

        size_t fromTagDataSize = getTagDataSize(fromMap, fromTagHandle);
        if (fromTagDataSize == 0) return nullptr;

        void* toTagPath = toMap->allocate(strlen(fromTagPath) + 1);
        if (!toTagPath) return nullptr;
        memcpy(toTagPath, fromTagPath, strlen(fromTagPath) + 1);

        void* tagData = toMap->allocate(fromTagDataSize);
        if (!tagData) {
            toMap->free(toTagPath);
            return nullptr;
        }

        if (!copyTagData(schema, fromMap, toMap, fromTagHandle, tagData, fromTagDataSize)) {
            toMap->free(toTagPath);
            toMap->free(tagData);
            return nullptr;
        }

        Tag* tag = toMap->allocateTag();
        if (!tag) {
            toMap->free(toTagPath);
            toMap->free(tagData);
            return nullptr;
        }

        uint32_t toTagHandle = tag->tagID;
        memcpy(tag, fromTag, sizeof(Tag));
        tag->tagID = toTagHandle;
        tag->data = toMap->toRelative<PointerBase_Tags>(tagData);
        tag->path = toMap->toRelative<PointerBase_Tags>(toTagPath);

        return tag;
    }

}
