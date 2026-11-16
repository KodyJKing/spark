#include "structure_bsp.hpp"
#include "map_file.hpp"
#include "schema/functions.hpp"
#include "engine/tags/tag_group_id.hpp"

#include "iostream"
#include <stack>

namespace Engine::Map {

    StructureBspReference* getStructureBspReference(MapFile* map, Tag* t) {
        if (!map || !t) return nullptr;
        if (t->groupID != GroupId_BSP) return nullptr;
        Tag* firstTag = map->getTag(0);
        if (!firstTag) return nullptr;
        if (firstTag->groupID != GroupId_Scenario) return nullptr;
        Scenario* scenario = (Scenario*)map->getTagData(firstTag);
        if (!scenario) return nullptr;

        for (size_t i = 0; i < scenario->structureBsps.count; i++) {
            StructureBspReference* ref = map->getBlockElement(scenario->structureBsps, i);
            if (ref->tagReference.tagHandle == t->tagHandle) {
                return ref;
            }
        }
        return nullptr;
    }

    void fixPointerDisplacement(StructureRef ref, int64_t displacement) {
        
        FieldRef field = {0};
        field.parent = ref;

        for (const auto& pair : ref.node->fields) {
            const auto& fieldId = pair.second;
            if (!ref.context->schema->fields.contains(fieldId)) {
                continue;
            }
            auto& fieldNode = ref.context->schema->fields.at(fieldId);

            field.context = ref.context;
            field.node = &fieldNode;
            field.address = (char*)ref.address + fieldNode.offset;

            if (field.is(Type::DataPointer)) {
                DataPointer* dp = (DataPointer*) field.address;
                if (dp) {
                    dp->data.offset += displacement;
                }
            }
            
            if (field.is(Type::Structure)) {
                StructureRef structRef = field.getStructure();
                fixPointerDisplacement(structRef, displacement);
            }
    
            if (field.is(Type::BlockPointer)) {
                BlockPointer* bp = (BlockPointer*) field.address;
                if (bp) {
                    bp->data.offset += displacement;
                    size_t count = field.getBlockElementCount();
                    StructureRef elementRef = field.getBlockElement(0);

                    if (!needsProcessing(elementRef)) {
                        continue;
                    }

                    for (uint32_t i = 0; i < count; i++) {
                        fixPointerDisplacement(elementRef, displacement);
                        elementRef.address =  (char*)elementRef.address + elementRef.node->size;
                    }
                }
            }

        }

    }


    void normalizeStructureBspTag(Tag* t, Context* context) {
        if (!context) return;
        Schema* schema = context->schema;
        if (!schema) return;
        StructureNode* bspStructureNode = schema->structureForGroupId(GroupId_BSP);
        if (!bspStructureNode) return;
        
        MapFile* map = context->mapFile;
        if (!map || !t) return;
        if (t->groupID != GroupId_BSP) return;
        
        StructureBspReference* ref = getStructureBspReference(map, t);
        if (!ref) return;
        
        char* header = (char*)map->fromRelative(ref->structureBspHeader);
        StructureBspHeader* bspHeader = (StructureBspHeader*)header;

        char* actualBspData = header + sizeof(StructureBspHeader);
        char* expectedBspData = (char*)map->fromRelative(bspHeader->structureBsp);

        int64_t displacement = actualBspData - expectedBspData;
        if (displacement != 0) {
            StructureRef bspRef = {0};
            bspRef.context = context;
            bspRef.node = bspStructureNode;
            bspRef.address = actualBspData;
            fixPointerDisplacement(bspRef, displacement);
        }
    }

}
