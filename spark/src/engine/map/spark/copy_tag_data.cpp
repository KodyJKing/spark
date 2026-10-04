#include "copy_tag.hpp"
#include "get_tag_data_size.hpp"
#include "../schema/schema.hpp"
#include "../schema/reference.hpp"
#include "engine/map.hpp"
#include "utils/Strings.hpp"

#define DEBUG 1

#ifdef DEBUG
#include <iostream>
#define LOG(msg) std::cout << "[Engine::Map::Spark - copy_tag_data] " << msg << std::endl;
#else
#define LOG(msg)
#endif

namespace Engine::Map::Spark {

    //////////////////////////////////////
    void applyRelocationFixups(StructureRef fromStructure, StructureRef toStructure);
    void applyRelocationFixupsInner(RuntimeMapFile* toMap, StructureRef fromStructure, int64_t relocationOffset, int64_t dataOffset);
    //////////////////////////////////////
    
    bool copyTagData(
        Schema* schema,
        MapFile* fromMap,
        RuntimeMapFile* toMap,
        uint32_t fromTagHandle,
        // Optional, patched if possible. Will allocate new data block if not.
        void* toTagData = nullptr,
        size_t toTagDataSize = 0
    ) {
        Tag* fromTag = fromMap->getTag(fromTagHandle);
        if (!fromTag) return false;
        StructureNode* tagStruct = schema->structureForGroupId(fromTag->groupID);
        if (!tagStruct) return false;
        
        size_t fromTagDataSize = getTagDataSize(fromMap, fromTagHandle);
        if (!toTagData || toTagDataSize < fromTagDataSize) {
            toTagData = Engine::allocateMapMemory(fromTagDataSize);
            toTagDataSize = fromTagDataSize;

            if (!toTagData) {
                // Allocation failed, cannot proceed.
                return false;
            }
        }

        void* fromTagData = fromMap->getTagData(fromTag);
        if (!fromTagData) return false;

        memcpy(toTagData, fromTagData, fromTagDataSize);

        Context fromContext = { fromMap, schema };
        StructureRef fromStructure = {
            &fromContext, 
            tagStruct,
            fromTagData
        };
        
        Context toContext = { toMap, schema };
        StructureRef toStructure = {
            &toContext,
            tagStruct,
            toTagData
        };

        applyRelocationFixups(fromStructure, toStructure);
        return true;
    }

    uint32_t getTagHeaderOffset(StructureRef ref) {
        auto pointer = ref.context->mapFile->toRelative<PointerBase_Tags>(ref.address);
        return pointer.offset;
    }

    // How far is the structure moving relative to its map's tag header?
    int64_t getRelocationOffset(StructureRef fromStructure, StructureRef toStructure) {
        uint32_t fromOffset = getTagHeaderOffset(fromStructure);
        uint32_t toOffset = getTagHeaderOffset(toStructure);
        return static_cast<int64_t>(toOffset) - static_cast<int64_t>(fromOffset);
    }

    template<typename T>
    T* getDestination(T* source, int64_t dataOffset) {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(source) + dataOffset);
    }

    void applyRelocationFixups(
        StructureRef fromStructure,
        StructureRef toStructure
    ) {
        int64_t relocationOffset = getRelocationOffset(fromStructure, toStructure);
        int64_t dataOffset = (char*)toStructure.address - (char*)fromStructure.address;
        applyRelocationFixupsInner((RuntimeMapFile*)toStructure.context->mapFile, fromStructure, relocationOffset, dataOffset);
    }

    void applyRelocationFixupsInner(
        RuntimeMapFile* toMap,
        StructureRef fromStructure,
        int64_t relocationOffset,
        int64_t dataOffset
    ) {
        // Iterate over the fields in the structure and apply relocation fixups as needed.
        // This typically involves adjusting pointers within the tag data to account for the new memory location.
        for (auto& field : fromStructure.getFieldRefs()) {

            if (field.is(Type::BlockPointer)) {
                
                BlockPointer* fromPtr = reinterpret_cast<BlockPointer*>(field.address);
                BlockPointer* toPtr = getDestination(fromPtr, dataOffset);
                toPtr->data.offset += relocationOffset;

                // Recurse on each element of the block.
                for (size_t i = 0; i < fromPtr->count; ++i) {
                    StructureRef fromElement = field.getBlockElement(i);
                    applyRelocationFixupsInner(toMap, fromElement, relocationOffset, dataOffset);
                }
                
            } else if (field.is(Type::Structure)) {

                // Recurse on the nested structure.
                StructureRef fromNested = field.getStructure();
                applyRelocationFixupsInner(toMap, fromNested, relocationOffset, dataOffset);

            } else if (field.is(Type::TagReference)) {

                TagReference* tagRef = reinterpret_cast<TagReference*>(field.address);
                if (tagRef->tagHandle == 0xFFFFFFFF)
                    continue;

                uint32_t groupId = field.getTagGroupId();
                if (groupId == GroupId_Invalid) continue;
                
                const char* tagPath = field.getTagPath();
                if (!tagPath) continue;
                
                // Lookup the corresponding tag in the destination map.
                Tag* matchingTag = toMap->findTag(groupId, tagPath);
                bool tagCopied = false;

                std::string fullTagDescription = std::string("[") + Strings::fourccToString(groupId) + "] " + tagPath;
                if (!matchingTag) {
                    LOG("No matching tag found for path:" << fullTagDescription);
                    LOG("Attempting to copy tag for path:" << fullTagDescription);
                    MapFile* fromMap = fromStructure.context->mapFile;
                    uint32_t fromTagHandle = tagRef->tagHandle;
                    Tag* newTag = toMap->copyTag(fromMap, fromTagHandle);
                    if (!newTag) {
                        LOG("Could not copy tag for path:" << fullTagDescription);
                        continue;
                    }
                    matchingTag = newTag;
                    tagCopied = true;
                    LOG("Successfully copied tag for path:" << fullTagDescription);
                } else {
                    LOG("Found existing matching tag for path:" << fullTagDescription);
                }
                
                if (tagRef) {
                    tagRef->tagHandle = matchingTag->tagHandle;
                    tagRef->path = matchingTag->path;
                    tagRef->groupID = groupId;
                }

                if (!tagCopied) {
                    // Todo: Optionally patch the tag's data. Allow dev to specify which tags are patchable.
                }

            }

        }
    }

}
