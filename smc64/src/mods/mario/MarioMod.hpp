#pragma once
#include "spark/mod/IMod.hpp"

class MarioMod : public Spark::IMod {
public:
    void init() override;
    void free() override;
};
