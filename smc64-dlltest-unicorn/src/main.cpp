#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "TestRunner.hpp"

std::vector<TestCase>& UnicornTestRegistry() {
    static std::vector<TestCase> tests;
    return tests;
}

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool matchesAnyFilter(const std::string& lowerName, const std::vector<std::string>& lowerFilters) {
    if (lowerFilters.empty()) return true;
    for (const auto& f : lowerFilters) {
        if (lowerName.find(f) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    // Any command-line args are treated as case-insensitive substring
    // filters against test names, OR'd together -- e.g.
    // `smc64-dlltest-unicorn.exe LineTest RotateVec` runs only tests whose
    // name contains "linetest" or "rotatevec". No args (the default) runs
    // everything, same as before.
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        filters.push_back(toLower(argv[i]));
    }

    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (auto& test : UnicornTestRegistry()) {
        if (!matchesAnyFilter(toLower(test.name), filters)) {
            ++skipped;
            continue;
        }

        std::cout << "[ RUN  ] " << test.name << "\n";

        bool ok = false;
        try {
            ok = test.run();
        } catch (const std::exception& e) {
            std::cout << "  threw: " << e.what() << "\n";
        }

        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << test.name << "\n\n";
        ok ? ++passed : ++failed;
    }

    std::cout << passed << " passed, " << failed << " failed";
    if (skipped > 0) {
        std::cout << " (" << skipped << " skipped by filter)";
    }
    std::cout << "\n";
    return failed == 0 ? 0 : 1;
}
