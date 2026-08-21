#pragma once

#include "ButtonCode.hpp"
#include "spark/SparkAPI.h"

namespace Spark::Input {

    typedef unsigned int ButtonCode;

    // Button code namespace:
    // 0-255:   Keyboard (DIK_ scan code index)
    // 256-511: Mouse (0-7: buttons via GetAsyncKeyState)
    // 512-767: Gamepad
    //   0-15:  wButtons bit index
    //   16-17: LT / RT  (axes, 0-1)
    //   18-19: Left stick X- / X+
    //   20-21: Left stick Y- / Y+
    //   22-23: Right stick X- / X+
    //   24-25: Right stick Y- / Y+
    // An axis ButtonCode reads as "pressed" when its value >= SPARK_AXIS_PRESS_THRESHOLD.

    SPARK_API char*         getButtonName(ButtonCode button);
    SPARK_API float         getAxis(ButtonCode button);  // returns 0-1 for buttons or axis slots
    SPARK_API unsigned char actionStateRaw(ButtonCode button); // 0x80 if pressed

    extern unsigned char buttons[768];
    // 10 floats: LT, RT, LX-, LX+, LY-, LY+, RX-, RX+, RY-, RY+
    extern float axes[10];

    void init();
    void free();
    void update();

}
