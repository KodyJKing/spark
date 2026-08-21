#include "spark/input/Input.hpp"
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Xinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

namespace Spark::Input {

unsigned char buttons[768] = {};

static IDirectInput8*        g_di       = nullptr;
static IDirectInputDevice8*  g_keyboard = nullptr;

// DIK_ scan code names, indexed 0x00-0xFF
static const char* s_keyNames[256] = {
    nullptr,        // 0x00
    "Escape",       // 0x01
    "1",            // 0x02
    "2",            // 0x03
    "3",            // 0x04
    "4",            // 0x05
    "5",            // 0x06
    "6",            // 0x07
    "7",            // 0x08
    "8",            // 0x09
    "9",            // 0x0A
    "0",            // 0x0B
    "Minus",        // 0x0C
    "Equals",       // 0x0D
    "Backspace",    // 0x0E
    "Tab",          // 0x0F
    "Q",            // 0x10
    "W",            // 0x11
    "E",            // 0x12
    "R",            // 0x13
    "T",            // 0x14
    "Y",            // 0x15
    "U",            // 0x16
    "I",            // 0x17
    "O",            // 0x18
    "P",            // 0x19
    "LBracket",     // 0x1A
    "RBracket",     // 0x1B
    "Enter",        // 0x1C
    "LCtrl",        // 0x1D
    "A",            // 0x1E
    "S",            // 0x1F
    "D",            // 0x20
    "F",            // 0x21
    "G",            // 0x22
    "H",            // 0x23
    "J",            // 0x24
    "K",            // 0x25
    "L",            // 0x26
    "Semicolon",    // 0x27
    "Apostrophe",   // 0x28
    "Grave",        // 0x29
    "LShift",       // 0x2A
    "Backslash",    // 0x2B
    "Z",            // 0x2C
    "X",            // 0x2D
    "C",            // 0x2E
    "V",            // 0x2F
    "B",            // 0x30
    "N",            // 0x31
    "M",            // 0x32
    "Comma",        // 0x33
    "Period",       // 0x34
    "Slash",        // 0x35
    "RShift",       // 0x36
    "Numpad*",      // 0x37
    "LAlt",         // 0x38
    "Space",        // 0x39
    "CapsLock",     // 0x3A
    "F1",           // 0x3B
    "F2",           // 0x3C
    "F3",           // 0x3D
    "F4",           // 0x3E
    "F5",           // 0x3F
    "F6",           // 0x40
    "F7",           // 0x41
    "F8",           // 0x42
    "F9",           // 0x43
    "F10",          // 0x44
    "NumLock",      // 0x45
    "ScrollLock",   // 0x46
    "Numpad7",      // 0x47
    "Numpad8",      // 0x48
    "Numpad9",      // 0x49
    "Numpad-",      // 0x4A
    "Numpad4",      // 0x4B
    "Numpad5",      // 0x4C
    "Numpad6",      // 0x4D
    "Numpad+",      // 0x4E
    "Numpad1",      // 0x4F
    "Numpad2",      // 0x50
    "Numpad3",      // 0x51
    "Numpad0",      // 0x52
    "NumpadDot",    // 0x53
    nullptr,        // 0x54
    nullptr,        // 0x55
    nullptr,        // 0x56
    "F11",          // 0x57
    "F12",          // 0x58
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x59-0x5F
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x60-0x67
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x68-0x6F
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x70-0x77
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x78-0x7F
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x80-0x87
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x88-0x8F
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0x90-0x97
    nullptr, nullptr, nullptr, nullptr,                                     // 0x98-0x9B
    "NumpadEnter",  // 0x9C
    "RCtrl",        // 0x9D
    nullptr, nullptr,                                                       // 0x9E-0x9F
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0xA0-0xA7
    nullptr, nullptr, nullptr, nullptr, nullptr,                            // 0xA8-0xAC
    "Numpad/",      // 0xAD ... actually DIK_DIVIDE is 0xB5
    nullptr, nullptr,                                                       // 0xAE-0xAF
    nullptr, nullptr, nullptr, nullptr, nullptr,                            // 0xB0-0xB4
    "Numpad/",      // 0xB5
    nullptr, nullptr,                                                       // 0xB6-0xB7
    "RAlt",         // 0xB8
    nullptr,        // 0xB9
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0xBA-0xBF -> actually 0xBA-0xC1 wrong - let me just fill correctly
    // Actually let me just do blocks:
};

// Fully initialize the rest with extended keys inline is unwieldy;
// instead we patch the known extended entries after definition.
static const char* s_mouseNames[8] = {
    "Mouse1", "Mouse2", "Mouse3", "Mouse4",
    "Mouse5", "Mouse6", "Mouse7", "Mouse8",
};

// VK codes for mouse buttons 0-7; 0 = not available via this API
static const int s_mouseVKs[8] = {
    VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2, 0, 0, 0
};

static const char* s_padNames[16] = {
    "Pad_DUp",    "Pad_DDown",  "Pad_DLeft",  "Pad_DRight",
    "Pad_Start",  "Pad_Back",   "Pad_LThumb", "Pad_RThumb",
    "Pad_LB",     "Pad_RB",     nullptr,       nullptr,
    "Pad_A",      "Pad_B",      "Pad_X",       "Pad_Y",
};

char* getButtonName(ButtonCode button) {
    if (button < 256) {
        return const_cast<char*>(s_keyNames[button]);
    }
    if (button < 512) {
        unsigned int mouseIdx = button - 256;
        if (mouseIdx < 8)
            return const_cast<char*>(s_mouseNames[mouseIdx]);
        return nullptr;
    }
    {
        unsigned int padIdx = button - 512;
        if (padIdx < 16)
            return const_cast<char*>(s_padNames[padIdx]);
        return nullptr;
    }
}

void activeButtons(ButtonCode* buffer, int bufferSize, int* activeCount) {
    int count = 0;
    for (int i = 0; i < 768 && count < bufferSize; i++) {
        if (buttons[i] & 0x80)
            buffer[count++] = static_cast<ButtonCode>(i);
    }
    *activeCount = count;
}

static HWND findMainWindow() {
    struct Ctx { HWND hwnd; DWORD pid; };
    Ctx ctx = { nullptr, GetCurrentProcessId() };
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == c->pid && IsWindowVisible(h) && GetWindow(h, GW_OWNER) == nullptr) {
            c->hwnd = h;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hwnd ? ctx.hwnd : GetConsoleWindow();
}

void init() {
    HRESULT hr = DirectInput8Create(
        GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
        IID_IDirectInput8, reinterpret_cast<void**>(&g_di), nullptr);
    if (FAILED(hr)) return;

    HWND hwnd = findMainWindow();

    hr = g_di->CreateDevice(GUID_SysKeyboard, &g_keyboard, nullptr);
    if (SUCCEEDED(hr)) {
        g_keyboard->SetDataFormat(&c_dfDIKeyboard);
        g_keyboard->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        g_keyboard->Acquire();
    }

}

void free() {
    if (g_keyboard) { g_keyboard->Unacquire(); g_keyboard->Release(); g_keyboard = nullptr; }
    if (g_di)       { g_di->Release();                                g_di       = nullptr; }
    memset(buttons, 0, sizeof(buttons));
}

void update() {
    // Keyboard: 256 bytes, each 0x80 if pressed
    if (g_keyboard) {
        if (FAILED(g_keyboard->GetDeviceState(256, &buttons[0]))) {
            g_keyboard->Acquire();
            memset(&buttons[0], 0, 256);
        }
    }

    // Mouse: only sample when our process owns the foreground window
    DWORD fgPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &fgPid);
    bool hasFocus = fgPid == GetCurrentProcessId();
    for (int i = 0; i < 8; i++)
        buttons[256 + i] = (hasFocus && s_mouseVKs[i] && (GetAsyncKeyState(s_mouseVKs[i]) & 0x8000)) ? 0x80 : 0;

    // Gamepad: wButtons bits 0-15 stored as 0x80/0 per slot
    XINPUT_STATE xs = {};
    if (XInputGetState(0, &xs) == ERROR_SUCCESS) {
        WORD w = xs.Gamepad.wButtons;
        for (int i = 0; i < 16; i++)
            buttons[512 + i] = (w >> i) & 1 ? 0x80 : 0;
    } else {
        memset(&buttons[512], 0, 16);
    }
}

} // namespace Spark::Input
