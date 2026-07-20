#include <iostream>

#include "TestRunner.hpp"

std::vector<TestCase>& UnicornTestRegistry() {
    static std::vector<TestCase> tests;
    return tests;
}

int main() {
    int passed = 0;
    int failed = 0;

    for (auto& test : UnicornTestRegistry()) {
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

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
