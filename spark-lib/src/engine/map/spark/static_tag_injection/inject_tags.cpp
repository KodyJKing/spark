#include "inject_tags.hpp"
#include "symbol_table.hpp"
#include "engine/tags/tag_group_id.hpp"
#include "engine/map/bitmap.hpp"
#include "engine/map/model.hpp"

#include <map>
#include <vector>

#include <iostream>
#define WARN(msg) std::cout << "Warning: " << msg << std::endl;

namespace Engine::Map::Spark {

    using Symbol = SymbolTable::Symbol;
    using SymbolEntry = SymbolTable::SymbolEntry;
    using SymbolType = SymbolTable::Type;

    struct Injector {
        RawMapFile* baseMap;
        RawMapFile* guestMap;
        TagList* guestTags;
        RawMapFile* hostMap;

        SymbolTable symbolTable;

        std::vector<uint8_t> tagPathsBuffer;
        std::vector<uint8_t> vertexBuffer;
        std::vector<uint8_t> indexBuffer;
        std::vector<uint8_t> bitmapDataBuffer;
        uint32_t copyToBuffer(std::vector<uint8_t>& buffer, const void* data, size_t size) {
            uint32_t offset = buffer.size();
            buffer.resize(offset + size);
            memcpy(buffer.data() + offset, data, size);
            return offset;
        }

        void* tagPaths = nullptr;
        void* vertexData = nullptr;
        void* indexData = nullptr;
        void* bitmapData = nullptr;

        size_t guestTagCapacity = 0;

        void processHostTags() {
            size_t hostTagCount = hostMap->getTagCount();
            for (size_t i = 0; i < hostTagCount; ++i) {
                Tag* hostTag = hostMap->getTag(i);
                processHostTag(hostTag);
            }
            
            size_t originalTagPathsSize = tagPathsBuffer.size();
            guestTagCapacity = originalTagPathsSize / sizeof(Engine::Map::Tag);
        }

        void processHostTag(Tag* hostTag) {
            const char* path = hostMap->getTagPath(hostTag);
            
            Symbol symbol;
            symbol.type = SymbolTable::Type::Tag;
            symbol.nameHash = SymbolTable::hashName(path);
            symbol.groupId = hostTag->groupID;

            createTagSymbol(hostTag, symbol);
            createAndLinkTagPathSymbol(hostTag, path);
            processHostTagData(hostTag, symbol);
        }

        void processGuestTags() {
            for (PortableTagId tagID : *guestTags) { 
                Tag* guestTag = findGuestTag(tagID);
                if (!guestTag) continue;
                processGuestTag(guestTag);
            }
        }

        void processGuestTag(Tag* guestTag) {
            auto header = hostMap->getTagDataHeader();
            Tag* tags = hostMap->getTag(0);

            if (header->tagCount >= guestTagCapacity) {
                WARN("Not enough capacity for new guest tag");
                return;
            }

            uint32_t newTagIndex = header->tagCount++;
            Tag* injectedTag = &tags[newTagIndex];
            uint32_t newTagHandle = 0xE1170000 | (newTagIndex & 0xFFFF);
            *injectedTag = *guestTag;
            injectedTag->tagHandle = newTagHandle;

            const char* path = guestMap->getTagPath(guestTag);
            
            Symbol symbol;
            symbol.type = SymbolTable::Type::Tag;
            symbol.nameHash = SymbolTable::hashName(path);
            symbol.groupId = injectedTag->groupID;

            createTagSymbol(injectedTag, symbol);
            createAndLinkTagPathSymbol(injectedTag, path);
            processGuestTagData(injectedTag, symbol);
        }

        void processGuestTagData(Tag* injectedTag, Symbol& symbol) {
            
        }

        void createTagSymbol(Tag* hostTag, Symbol& symbol) {
            auto& entry = symbolTable.getOrCreate(symbol);
            auto pointer = hostMap->toRelative<PointerBase_Tags>(hostTag);
            entry.offset = pointer.offset;
            entry.resolved = true;
        }

        void createAndLinkTagPathSymbol(Tag* hostTag, const char* path) {
            Symbol key;
            key.type = SymbolTable::Type::TagPath;
            key.nameHash = SymbolTable::hashName(path);
            
            auto entry = symbolTable.getOrCreate(key);
            if (!entry.resolved) {
                // Copy the tag path into the tag paths buffer
                size_t pathLength = strlen(path) + 1;
                entry.offset = copyToBuffer(tagPathsBuffer, path, pathLength);
                entry.resolved = true;
            }

            symbolTable.link(key, &hostTag->path);
        }

        void processHostTagData(Tag* hostTag, Symbol& symbol) {
            switch (hostTag->groupID) {
                case GroupId_Bitmap:
                    processHostBitmapTag(hostTag, symbol);
                    break;
                case GroupId_Model:
                    processHostModelTag(hostTag, symbol);
                    break;
                default:
                    // Todo: handle other tag types
                    break;
            }
        }

        void processHostBitmapTag(Tag* hostTag, Symbol& symbol) {
            Bitmap* hostBitmap = (Bitmap*) hostMap->getTagData(hostTag);
            if (!hostBitmap) {
                WARN("Failed to retrieve host bitmap data for tag: ", hostTag->tagHandle);
                return;
            }
            for (size_t i = 0; i < hostBitmap->bitmapData.count; i++) {
                BitmapData* hostBitmapData = hostMap->getBlockElement(hostBitmap->bitmapData, i);
                if (!hostBitmapData) {
                    WARN("Failed to retrieve bitmap data block for index: ", i);
                    continue;
                }

                void* data = hostMap->fromRelative(hostBitmapData->data);
                if (!data) {
                    WARN("Failed to resolve host bitmap data for index: ", i);
                    continue;
                }

                uint32_t offset = copyToBuffer(bitmapDataBuffer, data, hostBitmapData->dataSize);

                Symbol texBufferSymbol;
                texBufferSymbol.type = SymbolTable::Type::TextureBuffer;
                texBufferSymbol.nameHash = symbol.nameHash;
                texBufferSymbol.bitmapIndex = i;

                auto& entry = symbolTable.getOrCreate(texBufferSymbol);
                entry.offset = offset;
                entry.resolved = true;

                symbolTable.link(texBufferSymbol, &hostBitmapData->data);
            }
        }

        void processHostModelTag(Tag* hostTag, Symbol& symbol) {
            Model* model = (Model*) hostMap->getTagData(hostTag);
            for (size_t i = 0; i < model->geometries.count; i++) {
                Geometry* geometry = hostMap->getBlockElement(model->geometries, i);
                if (!geometry) {
                    WARN("Failed to retrieve geometry block for index: ", i);
                    continue;
                }

                for (size_t j = 0; j < geometry->parts.count; j++) {
                    Part* part = hostMap->getBlockElement(geometry->parts, j);
                    if (!part) {
                        WARN("Failed to retrieve part block for index: ", j);
                        continue;
                    }

                    void* vertexData = hostMap->fromRelative(part->union0.vertices);
                    if (!vertexData) {
                        WARN("Failed to resolve vertex data for part index: ", j);
                        continue;
                    } else {
                        copyToBuffer(vertexBuffer, vertexData, part->vertexDataSize());
                    }

                    void* indexData = hostMap->fromRelative(part->indices);
                    if (!indexData) {
                        WARN("Failed to resolve index data for part index: ", j);
                        continue;
                    } else {
                        copyToBuffer(indexBuffer, indexData, part->indexDataSize());
                    }

                    // No linking required for vertex and index buffers since they don't move relative to their pointer bases.
                }
            }
        }

        void copyMapBytes() {
            size_t baseMapSize = baseMap->getCacheHeader()->fileSize;
            size_t guestMapSize = guestMap->getCacheHeader()->fileSize;
            size_t hostMapSize = baseMapSize + guestMapSize;
            CacheHeader* newCache = (CacheHeader*) malloc(hostMapSize);
            memcpy(newCache, baseMap->getCacheHeader(), baseMapSize);
            hostMap->cache = newCache;
            newCache->fileSize = hostMapSize;
        }

        Tag* findGuestTag(PortableTagId tagId) {
            size_t guestMapTagCount = guestMap->getTagCount();
            for (size_t i = 0; i < guestMapTagCount; ++i) {
                Tag* guestTag = guestMap->getTag(i);
                if (!guestTag || tagId.groupId != guestTag->groupID) continue;
                if (strcmp(tagId.path, guestMap->getTagPath(guestTag)) == 0) {
                    return guestTag;
                }
            }
            return nullptr;
        }

        void copyBuffersToMap() {
            size_t baseMapSize = baseMap->getCacheHeader()->fileSize;

            CacheHeader* header = hostMap->getCacheHeader();
            void* tail = (void*)((uintptr_t)header + baseMapSize);

            auto append = [&](std::vector<uint8_t>& src) {
                memcpy(tail, src.data(), src.size());
                tail = (void*)((uintptr_t)tail + src.size());
            };

            tagPaths = tail;
            append(tagPathsBuffer);
            
            vertexData = tail;
            append(vertexBuffer);
            
            indexData = tail;
            append(indexBuffer);

            bitmapData = tail;
            append(bitmapDataBuffer);

            header->fileSize = (uintptr_t)tail - (uintptr_t)header;
        }

        void linkSymbols() {

            for (auto& pair : symbolTable.entries) {
                auto& entry = pair.second;
                
                for (void* referrer : entry.referrers) {
                    // Process each referrer as needed
                }

            }

        }

        void linkSymbol(SymbolEntry& entry, void* referrer) {
            switch(entry.symbol.type) {
                case SymbolType::Tag: {
                    // Handle tag symbol
                    
                    break;
                }
                default: {
                    WARN("Unknown symbol type encountered");
                    break;
                }
            }
        }

    };

    void injectTags(
        RawMapFile& baseMap, 
        RawMapFile& guestMap,
        TagList& guestTags, 
        RawMapFile& hostMap
    ) {
        Injector injector;
        injector.baseMap = &baseMap;
        injector.guestMap = &guestMap;
        injector.guestTags = &guestTags;
        injector.hostMap = &hostMap;

        injector.copyMapBytes();
        injector.processHostTags();
        injector.processGuestTags();
        injector.copyBuffersToMap();
        injector.linkSymbols();
    }

}
