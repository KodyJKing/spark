#pragma once
#include "spark/mod/IMod.hpp"

class FreecamMod : public Spark::IMod {
public:
    void init() override;

private:
    bool enabled_ = false;
    void update();
};
