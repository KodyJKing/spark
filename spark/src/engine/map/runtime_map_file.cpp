#pragma once

#include "map_file.hpp"
#include "engine/common.hpp"
#include "engine/rendering/model_data.hpp"
#include "engine/map.hpp"

namespace Engine::Map {

    void* RuntimeMapFile::getPointerBase(PointerBase b) {
        switch(b) {
            case PointerBase_Map: return (void*) (dllBase() + 0x2B22744);
            case PointerBase_Tags: return (void*) tagDataBase();
            case PointerBase_Vertices: return getModelDataPointer();
            case PointerBase_Indices: return (char*) getModelDataPointer() + getVertexDataSize();
            default: return nullptr;
        }
    }

    TagDataHeader* RuntimeMapFile::getTagDataHeader() {
        return (TagDataHeader*) tagHeaderBase();
    }

}
