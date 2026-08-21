#include "spark/input/Bindings.hpp"
#include "spark/input/Input.hpp"
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <Windows.h>
#include <filesystem>

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

    saveBindings();
}

void unbindAction(const char* actionName, ButtonCode button) {
    auto it = s_currentBindings.find(actionName);
    if (it == s_currentBindings.end()) return;
    auto& v = it->second;
    v.erase(std::remove(v.begin(), v.end(), button), v.end());

    saveBindings();
}

void resetAction(const char* actionName) {
    auto it = defaultBindings.find(actionName);
    if (it != defaultBindings.end())
        s_currentBindings[actionName] = it->second;
    
    saveBindings();
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

float actionAxis(const char* actionName) {
    auto it = s_currentBindings.find(actionName);
    if (it == s_currentBindings.end()) return 0.0f;
    float axis = 0.0f;
    for (ButtonCode btn : it->second) {
        if (btn < 768) axis += Input::getAxis(btn);
    }
    return axis;
}

static std::filesystem::path bindingsPath() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).parent_path() / "spark_bindings.txt";
}

void saveBindings() {
    std::ofstream f(bindingsPath());
    if (!f) return;
    for (auto& [name, codes] : s_currentBindings) {
        f << name;
        for (ButtonCode c : codes) f << ',' << c;
        f << '\n';
    }
}

void loadBindings() {
    std::ifstream f(bindingsPath());
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string token;
        std::getline(ss, token, ',');
        std::string name = token;
        std::vector<ButtonCode> codes;
        while (std::getline(ss, token, ',')) {
            codes.push_back(static_cast<ButtonCode>(std::stoul(token)));
        }
        s_currentBindings[name] = std::move(codes);
    }
}

} // namespace Spark::Input
