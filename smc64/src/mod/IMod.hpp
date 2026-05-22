#pragma once
#include "hook/ModId.hpp"

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
