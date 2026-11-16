#include <catch_amalgamated.hpp>
#include "InjectionFiles.hpp"

#include <fstream>

namespace {
    struct Files {
        std::filesystem::path directory;
        std::filesystem::path host;
        std::filesystem::path guest;
        std::filesystem::path output;

        Files() {
            GUID id;
            REQUIRE(SUCCEEDED(CoCreateGuid(&id)));
            wchar_t name[40];
            REQUIRE(StringFromGUID2(id, name, static_cast<int>(std::size(name))) != 0);
            directory = std::filesystem::temp_directory_path() / (std::wstring(L"spool-test-") + name);
            REQUIRE(std::filesystem::create_directory(directory));
            host = directory / "host.map";
            guest = directory / "guest.map";
            output = directory / "output.map";
            std::ofstream(host, std::ios::binary) << "host input";
            std::ofstream(guest, std::ios::binary) << "guest input";
        }

        ~Files() { std::filesystem::remove_all(directory); }
    };
}

TEST_CASE("Spool saves injection output separately and replaces only the chosen output", "[inject_tags][injection_files]") {
    Files files;
    auto originalHost = Spool::readInjectionMap(files.host);
    auto originalGuest = Spool::readInjectionMap(files.guest);
    std::vector<uint8_t> result{1, 2, 3, 0, 4};
    SECTION("new output") {}
    SECTION("existing output") { std::ofstream(files.output) << "old output"; }
    Spool::saveInjectedMap(files.output, result, files.host, files.guest);
    REQUIRE(Spool::readInjectionMap(files.output) == result);
    REQUIRE(Spool::readInjectionMap(files.host) == originalHost);
    REQUIRE(Spool::readInjectionMap(files.guest) == originalGuest);
    REQUIRE(std::distance(std::filesystem::directory_iterator(files.directory),
        std::filesystem::directory_iterator{}) == 3);
}

TEST_CASE("Spool refuses to overwrite either input or its aliases", "[inject_tags][injection_files]") {
    Files files;
    auto originalHost = Spool::readInjectionMap(files.host);
    auto originalGuest = Spool::readInjectionMap(files.guest);
    auto destination = files.host;
    SECTION("host") {}
    SECTION("guest") { destination = files.guest; }
    SECTION("case-insensitive alias") { destination = files.directory / "HOST.MAP"; }
    SECTION("relative component alias") { destination = files.directory / "." / "host.map"; }
    SECTION("hard link") {
        destination = files.output;
        std::filesystem::create_hard_link(files.host, destination);
    }
    REQUIRE_THROWS_WITH(Spool::saveInjectedMap(destination, std::vector<uint8_t>{9}, files.host, files.guest),
        "Choose a different output file. Injection cannot overwrite the host or guest map.");
    REQUIRE(Spool::readInjectionMap(files.host) == originalHost);
    REQUIRE(Spool::readInjectionMap(files.guest) == originalGuest);
}

TEST_CASE("Spool output failures preserve existing files and clean up temporary output", "[inject_tags][injection_files]") {
    Files files;
    auto original = Spool::readInjectionMap(files.host);
    SECTION("destination is a directory") {
        REQUIRE_THROWS_AS(Spool::saveInjectedMap(files.directory, std::vector<uint8_t>{9},
            files.host, files.guest), std::system_error);
    }
    SECTION("missing destination directory") {
        REQUIRE_THROWS_AS(Spool::saveInjectedMap(files.directory / "missing" / "output.map",
            std::vector<uint8_t>{9}, files.host, files.guest), std::system_error);
    }
    SECTION("empty result") {
        std::ofstream(files.output) << "existing";
        auto existing = Spool::readInjectionMap(files.output);
        REQUIRE_THROWS_AS(Spool::saveInjectedMap(files.output, {}, files.host, files.guest), std::runtime_error);
        REQUIRE(Spool::readInjectionMap(files.output) == existing);
    }
    SECTION("missing input") {
        REQUIRE_THROWS_AS(Spool::saveInjectedMap(files.output, std::vector<uint8_t>{9},
            files.host, files.directory / "missing.map"), std::system_error);
    }
    REQUIRE(Spool::readInjectionMap(files.host) == original);
    for (const auto& entry : std::filesystem::directory_iterator(files.directory))
        REQUIRE(entry.path().extension() != ".tmp");
}
