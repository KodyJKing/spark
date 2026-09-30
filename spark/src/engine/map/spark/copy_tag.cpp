#include "copy_tag.hpp"
#include "get_tag_data_size.hpp"
#include "../bitmap.hpp"

namespace Engine::Map::Spark {

    bool copyBitmapFixup(
        MapFile* fromMap,
        MapFile* toMap,
        SparkBitmap* toBitmap,
        uint32_t toBitmapTagHandle
    );

    bool applyTypedFixups(
        MapFile* fromMap,
        RuntimeMapFile* toMap,
        Tag* tag
    ) {
        if (!tag) return false;

        void* tagData = toMap->getTagData(tag);

        switch(tag->groupID) {
            case GroupId_Bitmap: return copyBitmapFixup(fromMap, toMap, (SparkBitmap*) tagData, tag->tagHandle);
        }

        return true;
    }

    // Todo: If a tag already exists with same groupId and path, patch it rather than allocating a new tag.
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

        uint32_t toTagHandle = tag->tagHandle;
        memcpy(tag, fromTag, sizeof(Tag));
        tag->tagHandle = toTagHandle;
        tag->data = toMap->toRelative<PointerBase_Tags>(tagData);
        tag->path = toMap->toRelative<PointerBase_Tags>(toTagPath);

        applyTypedFixups(fromMap, toMap, tag);

        return tag;
    }

}
