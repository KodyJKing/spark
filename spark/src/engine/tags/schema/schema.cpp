#include "schema.hpp"

#include "engine/tag.hpp"

#include "memory/Memory.hpp"

#include <cinttypes>

namespace Engine::TagSchema {

    std::string hex(uintptr_t value) {
        char buffer[17]; // 16 hex digits + null terminator, wide enough for 64-bit addresses
        snprintf(buffer, sizeof(buffer), "%016" PRIXPTR, value);
        return std::string(buffer);
    }

    std::string signedHex(intptr_t value) {
        char buffer[18]; // sign + 16 hex digits + null terminator
        if (value < 0) {
            snprintf(buffer, sizeof(buffer), "-%016" PRIXPTR, -value);
        } else {
            snprintf(buffer, sizeof(buffer), "+%016" PRIXPTR, value);
        }
        return std::string(buffer);
    }

    uintptr_t getRelocatedAddress(const Context& context, uintptr_t address) {
        return context.relocationOffset + address;
    }

    std::string Field::readString(const Context& context) {
        switch (type.primitive) {
            case PrimitiveTypeRef::Bit: {
                uint8_t bytes = *reinterpret_cast<const uint8_t*>(context.structureBase + offset);
                bool bit = bytes & (1 << bitOffset);
                return std::to_string(bit);
            }
            case PrimitiveTypeRef::Uint8:
                return std::to_string(*reinterpret_cast<const uint8_t*>(context.structureBase + offset));
            case PrimitiveTypeRef::Uint16:
                return std::to_string(*reinterpret_cast<const uint16_t*>(context.structureBase + offset));
            case PrimitiveTypeRef::Uint32:
                return std::to_string(*reinterpret_cast<const uint32_t*>(context.structureBase + offset));
            case PrimitiveTypeRef::Int8:
                return std::to_string(*reinterpret_cast<const int8_t*>(context.structureBase + offset));
            case PrimitiveTypeRef::Int16:
                return std::to_string(*reinterpret_cast<const int16_t*>(context.structureBase + offset));
            case PrimitiveTypeRef::Int32:
                return std::to_string(*reinterpret_cast<const int32_t*>(context.structureBase + offset));
            case PrimitiveTypeRef::Float:
                return std::to_string(*reinterpret_cast<const float*>(context.structureBase + offset));
            case PrimitiveTypeRef::TagString: {
                // TagString is stored as a fixed-size array of 32 bytes. Don't assume null-termination.
                const char* str = reinterpret_cast<const char*>(context.structureBase + offset);
                return std::string(str, strnlen(str, 32));
            }
            case PrimitiveTypeRef::TagReference: {
                auto tagId = *reinterpret_cast<const uint32_t*>(context.structureBase + offset);
                auto tag = Engine::getTag(tagId);
                if (Engine::tagExists(tag)) {
                    auto groupId = tag->classIdStr();
                    return std::string(tag->getResourcePath()) + " (" + groupId + ")";
                }
                return "Tag not found";
            }
            case PrimitiveTypeRef::StructureReference: {
                uint32_t address = Memory::safeRead<uint32_t>(context.structureBase + offset + 4).value_or(0);
                if (address == 0) return "<not allocated>";
                uintptr_t relocatedAddress = getRelocatedAddress(context, address);
                int64_t finalOffset = relocatedAddress - context.structureBase;
                std::string description = "Structure at offset " + hex(relocatedAddress) + "(" + signedHex(finalOffset) + ")";
                return description;
            }
            default:
                return "Not implemented";
        }
    }

    // Delete a field from a structure.
    void Structure::deleteField(Field* field) {
        auto it = std::find_if(fields.begin(), fields.end(), [&](const Field& f) { return &f == field; });
        if (it != fields.end()) {
            fields.erase(it);
        }
    }

}