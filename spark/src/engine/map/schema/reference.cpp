#include "reference.hpp"
#include "functions.hpp"
#include "schema.hpp"
#include "type.hpp"

#include "utils/Strings.hpp"
#include "memory/Memory.hpp"

#define DEBUG_REFERENCE

#ifdef DEBUG_REFERENCE
#include <iostream>
#define LOG(X) std::cout << "[DEBUG_REFERENCE] " << X << std::endl;
#else
#define LOG(X)
#endif

namespace Engine::Map {

    //////////////////////////////////////////////
    // StructureRef

    bool StructureRef::valid() const {
        return context != nullptr && address != nullptr;
    }

    bool StructureRef::allocated() const {
        return Memory::isAllocated(address);
    }

    bool FieldRef::valid() const {
        return context != nullptr && address != nullptr && parent.valid();
    }

    FieldRef StructureRef::getFieldRef(const char* name) {
        if (!valid()) {
            LOG("StructureRef is not valid");
            return FieldRef{};
        }
        
        for (const auto& pair : node->fields) {
            const auto& fieldId = pair.second;
            if (context->schema->fields.find(fieldId) == context->schema->fields.end()) {
                LOG("Field ID " << fieldId << " not found in schema");
                continue;
            }
            auto& field = context->schema->fields.at(fieldId);
            if (strncmp(field.name, name, 256) == 0) {
                return {
                    context,
                    &field,
                    (void*)((char*) this->address + field.offset),
                    *this
                };
            }
        }
        LOG("Field with name " << name << " not found");
        return FieldRef{};
    }

    std::vector<FieldRef> StructureRef::getFieldRefs() {
        if (!valid()) {
            LOG("StructureRef is not valid");
            return {};
        }
        
        
        std::vector<FieldRef> refs;
        for (const auto& pair : node->fields) {
            const auto& fieldId = pair.second;
            if (!context->schema->fields.contains(fieldId)) {
                LOG("Field ID " << fieldId << " not found in schema");
                continue;
            }
            auto& field = context->schema->fields.at(fieldId);
            refs.emplace_back(
                context,
                &field,
                (void*)((char*) this->address + field.offset),
                *this
            );
        }
        return refs;
    }

    void StructureRef::insertField(Id id) {
        if (!valid()) {
            LOG("StructureRef is not valid");
            return;
        }
        if (!context->schema->fields.contains(id)) {
            LOG("Field ID " << id << " not found in schema");
            return;
        }
        auto& field = context->schema->fields.at(id);
        node->fields[field.offset] = id;
    }

    void StructureRef::createFieldAt(void* address, Type type) {
        if (!valid()) {
            LOG("StructureRef is not valid");
            return;
        }
        if (!node->assertWritable("Cannot create field on read-only structure")) {
            return;
        }
        
        Schema* schema = context->schema;
        int offset = (int)((char*)address - (char*)this->address);
        std::string fieldName = "field_0x" + Strings::toHex(offset);

        FieldNode* fieldNode = nullptr;
        Id fieldId = schema->createField(fieldName, &fieldNode, type);
        if (!fieldNode) {
            LOG("Failed to create field node");
            return;
        }

        fieldNode->offset = offset;
        insertField(fieldId);
    }

    //////////////////////////////////////////////
    // FieldRef

    std::string FieldRef::readAsString() {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return "[invalid field reference]";
        }

        switch (node->type.type) {
            #define CONVERT(type) std::to_string(*(type*)address);
            case Type::U8: return CONVERT(uint8_t);
            case Type::U16: return CONVERT(uint16_t);
            case Type::U32: return CONVERT(uint32_t);
            case Type::U64: return CONVERT(uint64_t);
            case Type::I8: return CONVERT(int8_t);
            case Type::I16: return CONVERT(int16_t);
            case Type::I32: return CONVERT(int32_t);
            case Type::I64: return CONVERT(int64_t);
            case Type::F32: return CONVERT(float);
            case Type::F64: return CONVERT(double);
            case Type::BlockPointer: {
                void* structureBase = parent.address;
                StructureRef ref = getBlockElement(0);
                int64_t offset = (char*)ref.address - (char*)structureBase;
                std::string description = "Block at offset 0x" + Strings::toHex(offset);
                return description;
            }
            #undef CONVERT
            default:
                // LOG("Unsupported field type");
                return "[not implemented]";
        }
    }

    void FieldRef::writeFromString(const std::string& value) {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return;
        }

        LOG("Not implemented: writeFromString");
    }

    size_t FieldRef::size() const {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return 0;
        }
        int typeIndex = (int)node->type.type;
        if (typeIndex < 0 || typeIndex >= TypeCount) {
            LOG("Invalid type index: " << typeIndex);
            return 0;
        }
        return TypeSizes[typeIndex];
    }

    void FieldRef::deleteField() {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return;
        }
        parent.node->fields.erase(node->offset);
        context->schema->fields.erase(node->id);
    }

    bool FieldRef::createStructure() {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return false;
        }
        StructureNode* newNode = nullptr;
        Id newId = context->schema->createStructure("", &newNode);
        if (newId != NullId) {
            node->type.ref = newId;
            return true;
        }
        return false;
    }

    StructureNode *FieldRef::getBlockPointerType()
    {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return nullptr;
        }
        if (node->type.type != Type::BlockPointer) {
            LOG("FieldRef is not a block pointer");
            return nullptr;
        }
        if (!context->schema->structures.contains(node->type.ref)) {
            LOG("Block pointer type not found in schema");
            return &NullStructure;
        }
        return &context->schema->structures[node->type.ref];
    }

    StructureRef FieldRef::getBlockElement(size_t index) {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return StructureRef{nullptr};
        }
        if (node->type.type != Type::BlockPointer) {
            LOG("FieldRef is not a block");
            return StructureRef{nullptr};
        }

        StructureNode* structNode = getBlockPointerType();
        if (!structNode) {
            LOG("Block pointer type not found");
            return StructureRef{nullptr};
        }

        BlockPointer* blockPointer = (BlockPointer*)address;
        void* blockBase = context->mapFile->fromRelative(blockPointer->data);

        void* elementAddress = (char*)blockBase + index * structNode->size;

        return StructureRef{context, structNode, elementAddress};
    }

    bool FieldRef::is(Type type) {
        if (!valid()) {
            LOG("FieldRef is not valid");
            return false;
        }
        return node->type.type == type;
    }

}
