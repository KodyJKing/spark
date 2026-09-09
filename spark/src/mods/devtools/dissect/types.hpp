#pragma once

#include "engine/halo1.hpp"
#include "engine/tags/schema/schema.hpp"

#include <vector>
#include <cstdint>

namespace Mod::DevTools::DissectTag {
    using namespace Engine::TagSchema;

    struct WindowState {
        uint32_t currentTagId;
        bool open = true;
        bool hideClaimedBytes = true;
        
        uintptr_t baseAddress;

        ClaimedBytes claimedBytes = std::vector<bool>(4096);
        bool isClaimed(uintptr_t address) const {
            return address >= baseAddress && address < baseAddress + claimedBytes.size() && claimedBytes[address - baseAddress];
        }

        ClaimedBytes structureClaimedBytes = std::vector<bool>(4096);
        bool isStructureClaimed(uintptr_t address) const {
            return address >= baseAddress && address < baseAddress + structureClaimedBytes.size() && structureClaimedBytes[address - baseAddress];
        }

        uint64_t tick = 0;
    };

    struct RenderContext {
        WindowState* windowState;
        Engine::Tag* tag;
        TagSchema* schema;
        void* structureBase;
        Structure* structure;
        Field* field;
        size_t structureSize;
        
        bool* structureModified;

        RenderContext withTag(Engine::Tag* tag) {
            RenderContext context = *this;
            context.tag = tag;
            return context;
        }

        RenderContext withStructure(Structure* structure) {
            RenderContext context = *this;
            context.structure = structure;
            return context;
        }

        RenderContext withField(Field* field) {
            RenderContext context = *this;
            context.field = field;
            return context;
        }

        RenderContext withStructureBase(void* structureBase) {
            RenderContext context = *this;
            context.structureBase = structureBase;
            return context;
        }

        RenderContext withStructureModified(bool* structureModified) {
            RenderContext context = *this;
            context.structureModified = structureModified;
            return context;
        }

        RenderContext withStructureSize(size_t structureSize) {
            RenderContext context = *this;
            context.structureSize = structureSize;
            return context;
        }

        RenderContext withSchema(TagSchema* schema) {
            RenderContext context = *this;
            context.schema = schema;
            return context;
        }
        
    };

}
