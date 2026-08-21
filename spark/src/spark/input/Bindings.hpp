#pragma once

#include "Input.hpp"
#include "spark/SparkAPI.h"

#include <string>
#include <vector>
#include <map>

namespace Spark::Input {
    SPARK_API void addAction(const char* actionName, const ButtonCode* defaultButtons, int count);
    inline void addAction(const char* actionName, ButtonCode defaultButton) { addAction(actionName, &defaultButton, 1); }
    inline void addAction(const char* actionName, const std::vector<ButtonCode>& defaultButtons) { addAction(actionName, defaultButtons.data(), static_cast<int>(defaultButtons.size())); }

    SPARK_API void bindAction(const char* actionName, ButtonCode button);
    SPARK_API void unbindAction(const char* actionName, ButtonCode button);
    SPARK_API void resetAction(const char* actionName);
    SPARK_API unsigned char actionState(const char* actionName);
    SPARK_API unsigned char actionPressed(const char* actionName, unsigned char* lastState);
    SPARK_API float actionAxis(const char* actionName);

    const std::vector<ButtonCode>& getBoundButtons(const char* actionName);
    extern std::map<std::string, std::vector<ButtonCode>> defaultBindings;

    void loadBindings();
    void saveBindings();
}
