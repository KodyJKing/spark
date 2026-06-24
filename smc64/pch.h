#ifndef PCH_H
#define PCH_H

// smc64 has its own Vec3i struct (math/Vectors.hpp).
// Prevent decomp/types.h from redefining it as a plain array typedef.
#define VEC3I_DEFINED

// MSVC does not support GCC's __attribute__ syntax. Shim it out so that
// libsm64's decomp headers (which use __attribute__((aligned(...)))) compile.
#ifdef _MSC_VER
#define __attribute__(x)
#endif

#include <iostream>
#include <iomanip>
#include <filesystem>
#include <string>
#include <sstream>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <unordered_set>
#include <set>
#include <exception>
#include <cstdio>
#include <memory>
#include <map>
#include <type_traits>
#include <optional>

#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

#endif //PCH_H