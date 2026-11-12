#pragma once

#include <unordered_map>
#include <vector>

namespace Engine::Map::Spark {

    struct SymbolTable {
        enum class Type { Tag, TagData, TagPath, VertexBuffer, IndexBuffer, TextureBuffer };

        struct Symbol {
            Type type;
            uint64_t nameHash = 0;
            uint32_t groupId = 0;
            uint32_t geometryIndex = 0;
            uint32_t partIndex = 0;
            uint32_t bitmapIndex = 0;

            inline bool operator==(const Symbol& other) const {
                return type == other.type &&
                       nameHash == other.nameHash &&
                       groupId == other.groupId &&
                       geometryIndex == other.geometryIndex &&
                       partIndex == other.partIndex &&
                       bitmapIndex == other.bitmapIndex;
            }

            struct Hash {
                inline size_t operator()(const Symbol& symbol) const {
                    std::hash<uint64_t> hashFn;
                    size_t hash = hashFn(static_cast<uint64_t>(symbol.type));
                    hash ^= hashFn(symbol.nameHash) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= hashFn(symbol.groupId) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= hashFn(symbol.geometryIndex) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= hashFn(symbol.partIndex) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= hashFn(symbol.bitmapIndex) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    hash ^= hashFn(symbol.nameHash) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    return hash;
                }
            };
        };

        struct SymbolEntry {
            Symbol symbol;
            std::vector<void*> referrers;

            bool resolved = false;
            uint32_t offset = 0;
        };

        std::unordered_map<Symbol, SymbolEntry, Symbol::Hash> entries;

        inline SymbolEntry& getOrCreate(Symbol const& key) {
            auto it = entries.find(key);
            if (it != entries.end()) {
                return it->second;
            }
            SymbolEntry& entry = entries[key];
            entry.symbol = key;
            return entry;
        }

        inline void link(Symbol& symbol, void* referrer) {
            auto it = entries.find(symbol);
            if (it != entries.end()) {
                it->second.referrers.push_back(referrer);
            }
        }

        static inline uint64_t hashName(const char* name) {
            uint64_t h = 1469598103934665603ULL; // FNV offset basis
            for (; *name; ++name) {
                h ^= static_cast<uint8_t>(*name);
                h *= 1099511628211ULL;           // FNV prime
            }
            return h;
        }

    };

}
