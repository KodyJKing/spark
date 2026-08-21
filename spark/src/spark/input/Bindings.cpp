#include "spark/input/Bindings.hpp"
#include "spark/input/Input.hpp"
#include <algorithm>
#include <vector>

namespace Spark::Input {

std::map<std::string, std::vector<ButtonCode>> defaultBindings;

static std::map<std::string, std::vector<ButtonCode>> s_currentBindings;
static const std::vector<ButtonCode> s_empty;

void addAction(const char* actionName, const ButtonCode* defaultButtons, int count) {
    defaultBindings[actionName].assign(defaultButtons, defaultButtons + count);
    if (!s_currentBindings.count(actionName))
        s_currentBindings[actionName].assign(defaultButtons, defaultButtons + count);
}

void bindAction(const char* actionName, ButtonCode button) {
    auto& v = s_currentBindings[actionName];
    if (std::find(v.begin(), v.end(), button) == v.end())
        v.push_back(button);
}

void unbindAction(const char* actionName, ButtonCode button) {
    auto it = s_currentBindings.find(actionName);
    if (it == s_currentBindings.end()) return;
    auto& v = it->second;
    v.erase(std::remove(v.begin(), v.end(), button), v.end());
}

void resetAction(const char* actionName) {
    auto it = defaultBindings.find(actionName);
    if (it != defaultBindings.end())
        s_currentBindings[actionName] = it->second;
}

const std::vector<ButtonCode>& getBoundButtons(const char* actionName) {
    auto it = s_currentBindings.find(actionName);
    return it != s_currentBindings.end() ? it->second : s_empty;
}

unsigned char actionState(const char* actionName) {
    auto it = s_currentBindings.find(actionName);
    if (it == s_currentBindings.end()) return 0;
    for (ButtonCode btn : it->second)
        if (btn < 768 && (buttons[btn] & 0x80)) return 0x80;
    return 0;
}

unsigned char actionPressed(const char* actionName, unsigned char * lastState) {
    unsigned char state = actionState(actionName);
    unsigned char pressed = state & ~(*lastState);
    *lastState = state;
    return pressed;
}

} // namespace Spark::Input
