#include "InjectionFiles.hpp"

#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>

namespace Spool {
    namespace {
        [[noreturn]] void fileError(const char* operation) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
        }

        struct CloseHandleDeleter {
            void operator()(void* handle) const { CloseHandle(handle); }
        };
        using FileHandle = std::unique_ptr<void, CloseHandleDeleter>;

        FileHandle openInputGuard(const std::filesystem::path& path) {
            auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) fileError("Cannot protect injection input");
            return FileHandle(handle);
        }

        struct TemporaryOutput {
            std::filesystem::path path;
            FileHandle handle;

            explicit TemporaryOutput(const std::filesystem::path& directory) {
                GUID id;
                auto hr = CoCreateGuid(&id);
                if (FAILED(hr))
                    throw std::system_error(static_cast<int>(hr), std::system_category(), "Cannot name injection output");
                wchar_t name[40];
                if (!StringFromGUID2(id, name, static_cast<int>(std::size(name))))
                    throw std::runtime_error("Cannot format injection output name");
                path = directory / (std::wstring(L".spool-") + name + L".tmp");
                auto raw = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
                if (raw == INVALID_HANDLE_VALUE) fileError("Cannot create temporary injection output");
                handle.reset(raw);
            }

            ~TemporaryOutput() {
                handle.reset();
                if (!path.empty() && !DeleteFileW(path.c_str())) {
                    auto error = GetLastError();
                    if (error != ERROR_FILE_NOT_FOUND)
                        std::cerr << "Spool: cannot remove temporary output " << path << ": " << error << '\n';
                }
            }
        };
    }

    std::vector<uint8_t> readInjectionMap(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot open injection map: " + path.string());
        auto size = file.tellg();
        if (size < 0 || static_cast<uint64_t>(size) > (std::numeric_limits<uint32_t>::max)())
            throw std::runtime_error("Invalid injection map size: " + path.string());
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Cannot read injection map: " + path.string());
        return bytes;
    }

    void saveInjectedMap(const std::filesystem::path& destination, std::span<const uint8_t> bytes,
        const std::filesystem::path& host, const std::filesystem::path& guest) {
        if (destination.empty()) throw std::runtime_error("No injection output path selected");
        if (bytes.empty()) throw std::runtime_error("Cannot save an empty injection result");
        auto hostGuard = openInputGuard(host);
        auto guestGuard = openInputGuard(guest);
        auto output = std::filesystem::absolute(destination);
        if (std::filesystem::exists(output) &&
            (std::filesystem::equivalent(output, host) || std::filesystem::equivalent(output, guest)))
            throw std::runtime_error("Choose a different output file. Injection cannot overwrite the host or guest map.");

        TemporaryOutput temporary(output.parent_path());
        size_t offset = 0;
        while (offset < bytes.size()) {
            auto remaining = bytes.size() - offset;
            auto count = static_cast<DWORD>((std::min)(remaining, size_t(1024 * 1024)));
            DWORD written = 0;
            if (!WriteFile(temporary.handle.get(), bytes.data() + offset, count, &written, nullptr))
                fileError("Cannot write injection output");
            if (!written) throw std::runtime_error("Writing injection output made no progress");
            offset += written;
        }
        if (!FlushFileBuffers(temporary.handle.get())) fileError("Cannot flush injection output");
        temporary.handle.reset();
        // Input guards deny writes and deletion until publication is complete.
        if (!MoveFileExW(temporary.path.c_str(), output.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            fileError("Cannot publish injection output");
        temporary.path.clear();
    }
}
