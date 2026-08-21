#pragma once

#include "ButtonCode.hpp"

namespace Spark::Input {

    typedef unsigned int ButtonCode;

    // Button code namespace:
    // 0-255:   Keyboard (Index into 256 byte state from GetDeviceState)
    // 256-511: 
    //        0-7: Mouse (Index into 8 byte state from GetDeviceState(...).rgbButtons)
    // 512-767: 
    //        0-15: Gamepad (Index into XINPUT_STATE.Gamepad.wButtons (bit index))
    
    char * getButtonName(ButtonCode button);

    void activeButtons(ButtonCode* buffer, int bufferSize, int* activeCount);

    extern unsigned char buttons[768];

    void init();
    void free();
    void update();

}
