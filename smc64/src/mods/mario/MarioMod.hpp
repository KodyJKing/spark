#pragma once
#include "mod/IMod.hpp"

class MarioMod : public IMod {
public:
    void init() override;
    void free() override;
};
