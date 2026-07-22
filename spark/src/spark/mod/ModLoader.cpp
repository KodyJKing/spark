#include "ModLoader.hpp"
#include "utils/Utils.hpp"

#include <filesystem>
#include <iostream>

namespace Spark {

    using ModLoadFn = void(*)();

    void ModLoader::loadAll() {
        namespace fs = std::filesystem;

        auto dir = Utils::getModsDirectory();
        std::error_code ec;
        if (!fs::exists(dir, ec) || ec) {
            std::cout << "[ModLoader] No mods directory at " << dir << ", skipping." << std::endl;
            return;
        }

        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file() || entry.path().extension() != ".dll")
                continue;

            const auto path = entry.path();
            HMODULE mod = LoadLibraryW(path.c_str());
            if (!mod) {
                std::cout << "[ModLoader] Failed to load " << path << " (error " << GetLastError() << ")" << std::endl;
                continue;
            }

            auto load = (ModLoadFn) GetProcAddress(mod, "spark_modLoad");
            if (!load) {
                std::cout << "[ModLoader] " << path << " has no spark_modLoad export, skipping." << std::endl;
                FreeLibrary(mod);
                continue;
            }

            load();
            modules_.push_back(mod);
            std::cout << "[ModLoader] Loaded " << path << std::endl;
        }
    }

    void ModLoader::unloadAll() {
        for (auto mod : modules_)
            FreeLibrary(mod);
        modules_.clear();
    }

}
