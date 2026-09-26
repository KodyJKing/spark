#pragma once

#include "map_file.hpp"
#include <memory>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

namespace Engine::Map {

    class ManagedMapFile : public MapFile {
        public:

        static std::shared_ptr<ManagedMapFile> create(std::filesystem::path f) {
            auto map = std::make_shared<ManagedMapFile>();

            std::ifstream file(f, std::ios::binary | std::ios::ate);
            if (!file) return nullptr;

            auto size = file.tellg();
            file.seekg(0);
            map->buffer.resize(static_cast<size_t>(size));
            if (!file.read(reinterpret_cast<char*>(map->buffer.data()), size)) return nullptr;

            map->raw = RawMapFile(reinterpret_cast<CacheHeader*>(map->buffer.data()));
            return map;
        }
        
        ManagedMapFile() = default;
        ~ManagedMapFile() = default;
        ManagedMapFile(const ManagedMapFile&) = delete;
        ManagedMapFile& operator=(const ManagedMapFile&) = delete;
        ManagedMapFile(ManagedMapFile&&) = delete;
        ManagedMapFile& operator=(ManagedMapFile&&) = delete;
        
        void* getPointerBase(PointerBase b) override {
            return raw.getPointerBase(b);
        }

        private:
        std::vector<uint8_t> buffer;
        RawMapFile raw;
    };

    using ManagedMapFilePtr = std::shared_ptr<ManagedMapFile>;

}
