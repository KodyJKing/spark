#pragma once
#include "spark/mod/ModId.hpp"

namespace Spark {
    class ModRegistry;

    class IMod {
    public:
        virtual ~IMod() = default;
        virtual void init() {}
        virtual void free() {}

    protected:
        ModId modId_ = 0;
        friend class ModRegistry;
    };
}
