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

#define CLAIM_BYTES_MAX_BLOCK_ITERATIONS 1024

namespace Engine::TagSchema {

    std::string hex(uintptr_t value) {
        char buffer[17]; // 16 hex digits + null terminator, wide enough for 64-bit addresses
        snprintf(buffer, sizeof(buffer), "%016" PRIXPTR, value);
        return std::string(buffer);
    }

    std::string signedHex(intptr_t value) {
        char buffer[18]; // sign + 16 hex digits + null terminator
        if (value < 0) {
            snprintf(buffer, sizeof(buffer), "-%X", -value);
        } else {
            snprintf(buffer, sizeof(buffer), "+%X8", value);
        }
        return std::string(buffer);
    }

    uintptr_t getRelocatedAddress(const Context& context, uintptr_t address) {
        return context.relocationOffset + address;
    }

    uintptr_t getStructureFieldAddress(const Context& context, Field& structureField) {
        size_t offset = structureField.offset;
        uint32_t address = Memory::safeRead<uint32_t>(context.structureBase + offset + 4).value_or(0);
        if (address == 0) return 0;
        return getRelocatedAddress(context, address);
    }

    std::string Field::readString(const Context& context) {
        switch (type.primitive) {
            case PrimitiveTypeRef::Bit: {
                uint8_t bytes = *reinterpret_cast<const uint8_t*>(context.structureBase + offset);
                bool bit = bytes & (1 << bitOffset);
                return std::to_string(bit);
            }

            #define CONVERT(type, adjustedOffset) std::to_string(*reinterpret_cast<const type*>(context.structureBase + offset + adjustedOffset))

            case PrimitiveTypeRef::Uint8:
                return CONVERT(uint8_t, 0);
            case PrimitiveTypeRef::Uint16:
                return CONVERT(uint16_t, 0);
            case PrimitiveTypeRef::Uint32:
                return CONVERT(uint32_t, 0);
            case PrimitiveTypeRef::Int8:
                return CONVERT(int8_t, 0);
            case PrimitiveTypeRef::Int16:
                return CONVERT(int16_t, 0);
            case PrimitiveTypeRef::Int32:
                return CONVERT(int32_t, 0);

            #define FLOAT(adjustedOffset) CONVERT(float, adjustedOffset)
            case PrimitiveTypeRef::Float:
                return FLOAT(0);
            case PrimitiveTypeRef::Vec3:
                return FLOAT(0) + ", " +
                       FLOAT(4) + ", " +
                       FLOAT(8);
            case PrimitiveTypeRef::Vec4:
                return FLOAT(0) + ", " +
                       FLOAT(4) + ", " +
                       FLOAT(8) + ", " +
                       FLOAT(12);
            case PrimitiveTypeRef::Matrix3x3:
                return FLOAT(0) + ", " +
                       FLOAT(4) + ", " +
                       FLOAT(8) + "; " +
                       FLOAT(12) + ", " +
                       FLOAT(16) + ", " +
                       FLOAT(20) + "; " +
                       FLOAT(24) + ", " +
                       FLOAT(28) + ", " +
                       FLOAT(32);
            #undef FLOAT
            #undef CONVERT
            
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
                uintptr_t relocatedAddress = getStructureFieldAddress(context, *this);
                if (relocatedAddress == 0) return "<not allocated>";
                int64_t finalOffset = relocatedAddress - context.structureBase;
                std::string description = "Structure at " + hex(relocatedAddress) + "(" + signedHex(finalOffset) + ")";
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

    void TagSchema::renameStructure(const std::string& oldName, const std::string& newName) {
        auto it = structures.find(oldName);
        if (it != structures.end()) {
            Structure structure = it->second;
            structure.name = newName;
            structures.erase(it);
            structures[newName] = structure;
        }
    }

}
