#pragma once

#ifdef SPARK_EXPORTS
#define SPARK_API __declspec(dllexport)
#else
#define SPARK_API __declspec(dllimport)
#endif

namespace Spark { class IMod; }

extern "C" {
    // Register a mod with Spark. Spark takes ownership; destroy() is called on teardown.
    // The mod must have been allocated in the calling module (destroy() calls delete this).
    SPARK_API void spark_registerMod(Spark::IMod* mod);

    // Hot-unload a previously registered mod.
    SPARK_API void spark_unregisterMod(Spark::IMod* mod);
}
