#pragma once
#include "spark/mod/ModId.hpp"

namespace Spark {
    class ModRegistry;

    class IMod {
    public:
        virtual ~IMod() = default;
        virtual void init() {}
        virtual void free() {}
        // Called by Spark to delete this mod. Override if allocation is not via operator new.
        // Default calls delete this, which runs in the allocating module's heap.
        virtual void destroy() { delete this; }

    protected:
        ModId modId_ = 0;
        friend class ModRegistry;
    };
}
