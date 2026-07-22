#include "ModLoader.hpp"
#include "utils/Utils.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

namespace Spark {

    using ModLoadFn = void(*)();

    namespace {

        // Directories to scan for mod DLLs: the default <exe-dir>/mods/ plus any extra
        // directories listed in SPARK_MODS_PATH (semicolon-delimited, like PATH). Lets a
        // mod repo's own build output be picked up directly during dev without a copy step.
        // Deduplicated by canonical path so overlapping/duplicate entries only get scanned once.
        std::vector<std::filesystem::path> getModDirectories() {
            namespace fs = std::filesystem;
            std::vector<fs::path> dirs;

            auto addDir = [&](const fs::path& dir) {
                std::error_code ec;
                fs::path canonical = fs::weakly_canonical(dir, ec);
                if (ec) canonical = dir;
                for (auto& existing : dirs)
                    if (existing == canonical) return;
                dirs.push_back(canonical);
            };

            addDir(Utils::getModsDirectory());

            if (const char* extraPaths = std::getenv("SPARK_MODS_PATH")) {
                std::string paths = extraPaths;
                size_t start = 0;
                while (start <= paths.size()) {
                    size_t end = paths.find(';', start);
                    if (end == std::string::npos) end = paths.size();
                    if (end > start)
                        addDir(paths.substr(start, end - start));
                    start = end + 1;
                }
            }

            return dirs;
        }

    }

    void ModLoader::loadAll() {
        namespace fs = std::filesystem;

        for (auto& dir : getModDirectories()) {
            std::error_code ec;
            if (!fs::exists(dir, ec) || ec) {
                std::cout << "[ModLoader] No mods directory at " << dir << ", skipping." << std::endl;
                continue;
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
    }

    void ModLoader::unloadAll() {
        for (auto mod : modules_)
            FreeLibrary(mod);
        modules_.clear();
    }

}

