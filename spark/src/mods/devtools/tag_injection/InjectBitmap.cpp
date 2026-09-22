#include "InjectBitmap.hpp"

#include "engine/halo1.hpp"
#include "engine/map_file/map_file.hpp"

#include <vector>

#include "imgui.h"

#include <iostream>

namespace {
    using namespace Engine::MapFile::TagData;
    
    Engine::MapFile::MapFile* loadedMap = nullptr;

    std::vector<uint8_t> textureBuffer;

    void loadMap(const char* filePath) {
        if (loadedMap) {
            delete loadedMap;
            loadedMap = nullptr;
        }
        loadedMap = new Engine::MapFile::MapFile();
        if (!loadedMap->loadFromFile(filePath)) {
            std::cout << "Failed to load map from file: " << filePath << std::endl;
            delete loadedMap;
            loadedMap = nullptr;
        }
    }

    void unloadMap() {
        if (loadedMap) {
            delete loadedMap;
            loadedMap = nullptr;
        }
    }

    Engine::MapFile::Tag* findTagInLoadedMap(const char* path, const char* groupId) {
        if (!loadedMap) return nullptr;
        for (uint32_t i = 0; i < loadedMap->getTagCount(); ++i) {
            auto pTag = loadedMap->getTag(i);
            if (!pTag) continue;
            auto& tag = *pTag;
            auto resourcePath = loadedMap->getTagPath(tag);
            std::cout << "Checking tag with resource path: " << resourcePath << " and group ID: " << std::string(tag.groupId, 4) << std::endl;
            if (strcmp(resourcePath, path) == 0 && memcmp(tag.groupId, groupId, 4) == 0) {
                return &tag;
            }
        }
        return nullptr;
    }

    int32_t copyTextureData(BitmapData* bitmap) {
        if (!bitmap) return -1;
        auto oldBitmapData = loadedMap->getTextureDataPointer(*bitmap);
        if (!oldBitmapData) return -1;

        // Copy texture data into textureBuffer
        size_t oldSize = textureBuffer.size();
        textureBuffer.resize(oldSize + bitmap->dataSize);
        memcpy(textureBuffer.data() + oldSize, oldBitmapData, bitmap->dataSize);
        return static_cast<int32_t>(oldSize);
    }

}

// Abandoning this. Keeping briefly for reference.
namespace Mod::DevTools::InjectBitmap {

    void init() {
        // Implementation for initializing data and hooks needed for bitmap injection.
    }

    uint32_t injectBitmapFromFile(const char* filePath, const char* tagName) {
        loadMap(filePath);

        auto fileTag = findTagInLoadedMap(tagName, "mtib");
        if (!fileTag) {
            std::cout << "Failed to find file tag for path: " << filePath << " with group ID: mitb" << std::endl;
            return NULL_HANDLE;
        }

        std::cout << "File tag found with ID: " << fileTag->tagID << " at data address: " << (void*)fileTag << std::endl;

        auto newTag = Engine::allocateTag({
            .groupId = Engine::GroupId_Bitmap,
            .parentGroupId = Engine::GroupId_Invalid,
            .grandparentGroupId = Engine::GroupId_Invalid,
            .resourcePath = tagName
        });
        if (!newTag) return NULL_HANDLE;
        std::cout << "New tag allocated with ID: " << newTag->tagID << " at data address: " << (void*)newTag << std::endl;

        // Naively copy the tag data. Assume 0 group sequences and 1 bitmap data (total size 220)
        void* newTagData = Engine::allocateTagData(220);
        if (!newTagData) return NULL_HANDLE;

        newTag->dataAddress = Engine::translateToMapAddress((uint64_t)newTagData);

        void* oldTagData = loadedMap->getTagData(*fileTag);

        int64_t relocationOffset = newTag->dataAddress - fileTag->dataPtr;
        std::cout << "Relocation offset: " << relocationOffset << std::endl;

        memcpy(newTagData, oldTagData, 220);

        using namespace Engine::MapFile;
        TagData::Bitmap* newBitmap = reinterpret_cast<TagData::Bitmap*>(newTagData);
        std::cout << "Bitmap data address: " << (void*)newBitmap << std::endl;

        TagData::Bitmap* oldBitmap = reinterpret_cast<TagData::Bitmap*>(oldTagData);

        auto newBitmapDataBlock = newBitmap->bitmapData();
        newBitmapDataBlock->offset += relocationOffset;

        // WRONG: Cannot load relative to loadedMap (old map).
        auto newBitmapData = loadedMap->resolveBlock(*newBitmapDataBlock);

        auto oldBitmapDataBlock = oldBitmap->bitmapData();
        auto oldBitmapData = loadedMap->resolveBlock(*oldBitmapDataBlock);
        if (!oldBitmapData || !newBitmapData) {
            std::cout << "Failed to resolve bitmap data block." << std::endl;
        } else {
            int32_t textureOffset = copyTextureData(oldBitmapData);
            std::cout << "Texture data copied to offset: " << textureOffset << std::endl;
            auto absoluteTextureAddress = textureBuffer.data() + textureOffset;
            std::cout << "Absolute texture address: " << (void*)absoluteTextureAddress << std::endl;
            newBitmapData->dataOffset = textureOffset;
        }

        unloadMap();
        
        return newTag->tagID;
    }

    void renderUI() {
        if (ImGui::Button("Load texture")) {
            injectBitmapFromFile(
                "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Halo The Master Chief Collection\\halo1\\maps\\d40.map", 
                "sky\\sky_d40\\skyofdoom\\bitmaps\\clouds doom tiled"
            );
        }
    }

}