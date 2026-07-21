#pragma once
#include "spark/mod/IMod.hpp"

class HookLogMod : public Spark::IMod {
public:
    void init() override;
    void free() override;
};
