#pragma once
#include "mod/IMod.hpp"

class FreecamMod : public IMod {
public:
    void init() override;

private:
    bool enabled_ = false;
    void update();
};
