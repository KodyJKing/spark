#include "schema.hpp"

#include "engine/tag.hpp"

#include "memory/Memory.hpp"

#include <cinttypes>

#define DEBUG_TAG_SCHEMA 1

#ifdef DEBUG_TAG_SCHEMA
#include <iostream>
#define LOG(x) std::cout << "[Engine::TagSchema] " << x << std::endl;
#else
#define LOG(x)
#endif

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

    void Field::writeString(const Context& context, const std::string& value) {
        try {
            switch (type.primitive) {
            case PrimitiveTypeRef::Uint8:
                *reinterpret_cast<uint8_t*>(context.structureBase + offset) = static_cast<uint8_t>(std::stoi(value));
                break;
            case PrimitiveTypeRef::Uint16:
                *reinterpret_cast<uint16_t*>(context.structureBase + offset) = static_cast<uint16_t>(std::stoi(value));
                break;
            case PrimitiveTypeRef::Uint32:
                *reinterpret_cast<uint32_t*>(context.structureBase + offset) = static_cast<uint32_t>(std::stoul(value));
                break;
            case PrimitiveTypeRef::Int8:
                *reinterpret_cast<int8_t*>(context.structureBase + offset) = static_cast<int8_t>(std::stoi(value));
                break;
            case PrimitiveTypeRef::Int16:
                *reinterpret_cast<int16_t*>(context.structureBase + offset) = static_cast<int16_t>(std::stoi(value));
                break;
            case PrimitiveTypeRef::Int32:
                *reinterpret_cast<int32_t*>(context.structureBase + offset) = static_cast<int32_t>(std::stoi(value));
                break;
            case PrimitiveTypeRef::Float:
                *reinterpret_cast<float*>(context.structureBase + offset) = std::stof(value);
                break;
            case PrimitiveTypeRef::TagString: {
                char* str = reinterpret_cast<char*>(context.structureBase + offset);
                strncpy(str, value.c_str(), 32);
                break;
            }
            case PrimitiveTypeRef::TagReference: {
                // Try parsing as a tag-handle
                uint32_t tagHandle = static_cast<uint32_t>(std::stoul(value));
                *reinterpret_cast<uint32_t*>(context.structureBase + offset) = tagHandle;
                // Todo: Try parsing as a tag path...
            }
            default:
                LOG("Unsupported field type for writing string: " << static_cast<int>(type.primitive));
                break;
            }
        }
        catch (const std::exception& e) {
            // Handle any conversion errors or other exceptions.
            LOG("Error writing string to field: " << e.what());
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