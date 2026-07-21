#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "UnicornEngine.hpp"

// Loads a captured memory-region "dump" into a fresh UnicornEngine.
//
// Manifest format (plain text, one region per line, '#' comments allowed):
//
//   <hex_base> <hex_or_dec_size> <perms:rwx, '-' for none> <bin_file|zero>
//
// Example:
//
//   # halo1.dll .text
//   0x00007ff9d8000000 0x00100000 r-x code.bin
//   # a thread's stack
//   0x0000004a12340000 0x00004000 rw- stack.bin
//   # scratch region -- content doesn't matter, just needs to be mapped
//   0x0000004a12350000 0x00001000 rw- zero
//
// - <bin_file> is a path relative to the manifest's own directory,
//   containing exactly <size> raw bytes to write into the region.
// - "zero" skips loading a file and leaves the mapped region zero-filled.
//
// Regions are rounded out to Unicorn's 4KB mapping granularity; regions
// captured from a real process (VirtualQuery / Cheat Engine memory regions)
// are already page-aligned, so this is normally a no-op.
class DumpLoader {
public:
    struct Region {
        uint64_t address;
        uint64_t size;
        uint32_t perms;
        std::string file;  // empty => zero-filled
    };

    static std::vector<Region> parseManifest(const std::filesystem::path& manifestPath) {
        std::ifstream in(manifestPath);
        if (!in) {
            throw std::runtime_error("cannot open manifest: " + manifestPath.string());
        }

        std::vector<Region> regions;
        std::string line;
        while (std::getline(in, line)) {
            std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            std::istringstream iss(trimmed);
            std::string addrStr, sizeStr, permsStr, fileStr;
            if (!(iss >> addrStr >> sizeStr >> permsStr >> fileStr)) {
                throw std::runtime_error("malformed manifest line: " + line);
            }

            Region region;
            region.address = std::stoull(addrStr, nullptr, 0);
            region.size = std::stoull(sizeStr, nullptr, 0);
            region.perms = parsePerms(permsStr);
            region.file = (fileStr == "zero") ? "" : fileStr;
            regions.push_back(std::move(region));
        }
        return regions;
    }

    // Loads every region in `manifestPath` into `engine`, resolving
    // relative file paths against the manifest's own parent directory.
    static void load(UnicornEngine& engine, const std::filesystem::path& manifestPath) {
        auto regions = parseManifest(manifestPath);
        auto dir = manifestPath.parent_path();

        for (auto& region : regions) {
            constexpr uint64_t kPageSize = 0x1000;
            uint64_t alignedBase = region.address & ~(kPageSize - 1);
            uint64_t end = region.address + region.size;
            uint64_t alignedEnd = (end + kPageSize - 1) & ~(kPageSize - 1);
            uint64_t alignedSize = alignedEnd - alignedBase;

            UnicornEngine::check(
                uc_mem_map(engine.handle(), alignedBase, alignedSize, region.perms),
                "uc_mem_map");

            if (!region.file.empty()) {
                std::ifstream bin(dir / region.file, std::ios::binary);
                if (!bin) {
                    throw std::runtime_error("cannot open region file: " + region.file);
                }
                std::vector<uint8_t> bytes(
                    (std::istreambuf_iterator<char>(bin)), std::istreambuf_iterator<char>());
                if (bytes.size() != region.size) {
                    throw std::runtime_error("region file size mismatch: " + region.file);
                }
                engine.memWrite(region.address, bytes.data(), bytes.size());
            }
        }
    }

private:
    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static uint32_t parsePerms(const std::string& p) {
        uint32_t perms = 0;
        if (p.find('r') != std::string::npos) perms |= UC_PROT_READ;
        if (p.find('w') != std::string::npos) perms |= UC_PROT_WRITE;
        if (p.find('x') != std::string::npos) perms |= UC_PROT_EXEC;
        return perms;
    }
};
