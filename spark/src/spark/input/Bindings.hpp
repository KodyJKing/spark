#pragma once

#include "Input.hpp"
#include "spark/SparkAPI.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace Spark::Input {
    SPARK_API void addAction(const char* actionName, const ButtonCode* defaultButtons, int count);
    inline void addAction(const char* actionName, ButtonCode defaultButton) { addAction(actionName, &defaultButton, 1); }
    SPARK_API void bindAction(const char* actionName, ButtonCode button);
    SPARK_API void unbindAction(const char* actionName, ButtonCode button);
    SPARK_API void resetAction(const char* actionName);
    SPARK_API unsigned char actionState(const char* actionName);
    SPARK_API unsigned char actionPressed(const char* actionName, unsigned char* lastState);

    const std::vector<ButtonCode>& getBoundButtons(const char* actionName);
    extern std::unordered_map<std::string, std::vector<ButtonCode>> defaultBindings;
}
