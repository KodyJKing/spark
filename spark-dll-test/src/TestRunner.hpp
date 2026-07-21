#pragma once

#include <eh.h>
#include <functional>
#include <string>
#include <vector>

#include "Halo1Module.hpp"

// Minimal test registry for calling real halo1.dll functions standalone.
//
// Usage (in a .cpp under tests/):
//
//   #include "TestRunner.hpp"
//
//   HALO1_TEST(LineTestVsBSP_MissesEmptyBSP) {
//       auto fn = halo1.at<...>(0x...);
//       // call fn(...), compare result to a hand-derived expected value
//       return actual == expected;
//   }
//
// Print details before returning false on failure -- the harness only
// reports PASS/FAIL, not *why*.

struct TestCase {
    std::string name;
    std::function<bool(const Halo1Module&)> run;
};

std::vector<TestCase>& Halo1TestRegistry();

struct TestRegistrar {
    TestRegistrar(std::string name, std::function<bool(const Halo1Module&)> fn) {
        Halo1TestRegistry().push_back({ std::move(name), std::move(fn) });
    }
};

#define HALO1_TEST(name)                                                              \
    static bool name(const Halo1Module& halo1);                                       \
    static TestRegistrar name##_registrar(#name, name);                               \
    static bool name(const Halo1Module& halo1)
