#pragma once

#include <functional>
#include <string>
#include <vector>

// Minimal, standalone test registry for Unicorn-based emulation tests.
//
// Deliberately separate from smc64-dlltest's HALO1_TEST/Halo1Module: these
// tests run against *captured memory dumps* (not committed to git -- see
// dumps/README.md) rather than a live-loaded halo1.dll, so the two suites
// build and run fully independently of each other.
//
// Usage (in a .cpp under tests/):
//
//   #include "TestRunner.hpp"
//
//   UNICORN_TEST(SomeFunction_SomeCondition_SomeExpectation) {
//       // build a UnicornEngine, load a dump (DumpLoader) or synthesize
//       // code directly, call into it via Win64Call, assert on the result.
//       return actual == expected;
//   }

struct TestCase {
    std::string name;
    std::function<bool()> run;
};

std::vector<TestCase>& UnicornTestRegistry();

struct TestRegistrar {
    TestRegistrar(std::string name, std::function<bool()> fn) {
        UnicornTestRegistry().push_back({ std::move(name), std::move(fn) });
    }
};

#define UNICORN_TEST(name)                                    \
    static bool name();                                       \
    static TestRegistrar name##_registrar(#name, name);        \
    static bool name()
