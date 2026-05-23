#define SPARK_EXPORTS
#include "spark/SparkAPI.h"
#include "spark/mod/ModRegistry.hpp"

namespace Spark {
    extern ModRegistry registry;
}

extern "C" {

void spark_registerMod(Spark::IMod* mod) {
    Spark::registry.add(mod);
}

void spark_unregisterMod(Spark::IMod* mod) {
    Spark::registry.unload(mod);
}

} // extern "C"
