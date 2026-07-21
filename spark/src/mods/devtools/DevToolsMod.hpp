#pragma once
#include "spark/mod/IMod.hpp"

class DevToolsMod : public Spark::IMod {
public:
    void init() override;
    void free() override;
};
