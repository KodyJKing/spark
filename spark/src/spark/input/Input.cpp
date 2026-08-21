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
    /* 00 */ nullptr,
    /* 01 */ "Escape",
    /* 02 */ "1",          /* 03 */ "2",          /* 04 */ "3",          /* 05 */ "4",
    /* 06 */ "5",          /* 07 */ "6",          /* 08 */ "7",          /* 09 */ "8",
    /* 0A */ "9",          /* 0B */ "0",          /* 0C */ "Minus",      /* 0D */ "Equals",
    /* 0E */ "Backspace",  /* 0F */ "Tab",
    /* 10 */ "Q",          /* 11 */ "W",          /* 12 */ "E",          /* 13 */ "R",
    /* 14 */ "T",          /* 15 */ "Y",          /* 16 */ "U",          /* 17 */ "I",
    /* 18 */ "O",          /* 19 */ "P",          /* 1A */ "LBracket",   /* 1B */ "RBracket",
    /* 1C */ "Enter",      /* 1D */ "LCtrl",
    /* 1E */ "A",          /* 1F */ "S",          /* 20 */ "D",          /* 21 */ "F",
    /* 22 */ "G",          /* 23 */ "H",          /* 24 */ "J",          /* 25 */ "K",
    /* 26 */ "L",          /* 27 */ "Semicolon",  /* 28 */ "Apostrophe", /* 29 */ "Grave",
    /* 2A */ "LShift",     /* 2B */ "Backslash",
    /* 2C */ "Z",          /* 2D */ "X",          /* 2E */ "C",          /* 2F */ "V",
    /* 30 */ "B",          /* 31 */ "N",          /* 32 */ "M",          /* 33 */ "Comma",
    /* 34 */ "Period",     /* 35 */ "Slash",       /* 36 */ "RShift",     /* 37 */ "Numpad*",
    /* 38 */ "LAlt",       /* 39 */ "Space",       /* 3A */ "CapsLock",
    /* 3B */ "F1",         /* 3C */ "F2",          /* 3D */ "F3",         /* 3E */ "F4",
    /* 3F */ "F5",         /* 40 */ "F6",          /* 41 */ "F7",         /* 42 */ "F8",
    /* 43 */ "F9",         /* 44 */ "F10",
    /* 45 */ "NumLock",    /* 46 */ "ScrollLock",
    /* 47 */ "Numpad7",    /* 48 */ "Numpad8",    /* 49 */ "Numpad9",    /* 4A */ "Numpad-",
    /* 4B */ "Numpad4",    /* 4C */ "Numpad5",    /* 4D */ "Numpad6",    /* 4E */ "Numpad+",
    /* 4F */ "Numpad1",    /* 50 */ "Numpad2",    /* 51 */ "Numpad3",    /* 52 */ "Numpad0",
    /* 53 */ "NumpadDot",
    /* 54 */ nullptr,      /* 55 */ nullptr,
    /* 56 */ "OEM102",
    /* 57 */ "F11",        /* 58 */ "F12",
    /* 59 */ nullptr,      /* 5A */ nullptr,      /* 5B */ nullptr,      /* 5C */ nullptr,
    /* 5D */ nullptr,      /* 5E */ nullptr,      /* 5F */ nullptr,
    /* 60 */ nullptr,      /* 61 */ nullptr,      /* 62 */ nullptr,      /* 63 */ nullptr,
    /* 64 */ "F13",        /* 65 */ "F14",        /* 66 */ "F15",
    /* 67 */ nullptr,      /* 68 */ nullptr,      /* 69 */ nullptr,      /* 6A */ nullptr,
    /* 6B */ nullptr,      /* 6C */ nullptr,      /* 6D */ nullptr,      /* 6E */ nullptr,
    /* 6F */ nullptr,
    /* 70 */ "Kana",
    /* 71 */ nullptr,      /* 72 */ nullptr,
    /* 73 */ "ABNT_C1",
    /* 74 */ nullptr,      /* 75 */ nullptr,      /* 76 */ nullptr,      /* 77 */ nullptr,
    /* 78 */ nullptr,
    /* 79 */ "Convert",
    /* 7A */ nullptr,
    /* 7B */ "NoConvert",
    /* 7C */ nullptr,
    /* 7D */ "Yen",        /* 7E */ "ABNT_C2",
    /* 7F */ nullptr,
    /* 80 */ nullptr,      /* 81 */ nullptr,      /* 82 */ nullptr,      /* 83 */ nullptr,
    /* 84 */ nullptr,      /* 85 */ nullptr,      /* 86 */ nullptr,      /* 87 */ nullptr,
    /* 88 */ nullptr,      /* 89 */ nullptr,      /* 8A */ nullptr,      /* 8B */ nullptr,
    /* 8C */ nullptr,
    /* 8D */ "NumpadEquals",
    /* 8E */ nullptr,      /* 8F */ nullptr,
    /* 90 */ "PrevTrack",
    /* 91 */ "At",         /* 92 */ "Colon",      /* 93 */ "Underline",
    /* 94 */ "Kanji",      /* 95 */ "Stop",       /* 96 */ "AX",         /* 97 */ "Unlabeled",
    /* 98 */ nullptr,
    /* 99 */ "NextTrack",
    /* 9A */ nullptr,      /* 9B */ nullptr,
    /* 9C */ "NumpadEnter",
    /* 9D */ "RCtrl",
    /* 9E */ nullptr,      /* 9F */ nullptr,
    /* A0 */ "Mute",       /* A1 */ "Calculator", /* A2 */ "PlayPause",
    /* A3 */ nullptr,
    /* A4 */ "MediaStop",
    /* A5 */ nullptr,      /* A6 */ nullptr,      /* A7 */ nullptr,      /* A8 */ nullptr,
    /* A9 */ nullptr,      /* AA */ nullptr,      /* AB */ nullptr,      /* AC */ nullptr,
    /* AD */ nullptr,
    /* AE */ "VolumeDown",
    /* AF */ nullptr,
    /* B0 */ "VolumeUp",
    /* B1 */ nullptr,
    /* B2 */ "WebHome",    /* B3 */ "NumpadComma",
    /* B4 */ nullptr,
    /* B5 */ "Numpad/",
    /* B6 */ nullptr,
    /* B7 */ "SysRq",      /* B8 */ "RAlt",
    /* B9 */ nullptr,      /* BA */ nullptr,      /* BB */ nullptr,      /* BC */ nullptr,
    /* BD */ nullptr,      /* BE */ nullptr,      /* BF */ nullptr,
    /* C0 */ nullptr,      /* C1 */ nullptr,      /* C2 */ nullptr,      /* C3 */ nullptr,
    /* C4 */ nullptr,
    /* C5 */ "Pause",
    /* C6 */ nullptr,
    /* C7 */ "Home",       /* C8 */ "Up",         /* C9 */ "PgUp",
    /* CA */ nullptr,
    /* CB */ "Left",
    /* CC */ nullptr,
    /* CD */ "Right",
    /* CE */ nullptr,
    /* CF */ "End",        /* D0 */ "Down",       /* D1 */ "PgDn",
    /* D2 */ "Insert",     /* D3 */ "Delete",
    /* D4 */ nullptr,      /* D5 */ nullptr,      /* D6 */ nullptr,      /* D7 */ nullptr,
    /* D8 */ nullptr,      /* D9 */ nullptr,      /* DA */ nullptr,
    /* DB */ "LWin",       /* DC */ "RWin",       /* DD */ "Apps",
    /* DE */ "Power",      /* DF */ "Sleep",
    /* E0 */ nullptr,      /* E1 */ nullptr,      /* E2 */ nullptr,
    /* E3 */ "Wake",
    /* E4 */ nullptr,
    /* E5 */ "WebSearch",  /* E6 */ "WebFavorites", /* E7 */ "WebRefresh",
    /* E8 */ "WebStop",    /* E9 */ "WebForward",   /* EA */ "WebBack",
    /* EB */ "MyComputer", /* EC */ "Mail",         /* ED */ "MediaSelect",
    /* EE */ nullptr,      /* EF */ nullptr,
    /* F0 */ nullptr,      /* F1 */ nullptr,      /* F2 */ nullptr,      /* F3 */ nullptr,
    /* F4 */ nullptr,      /* F5 */ nullptr,      /* F6 */ nullptr,      /* F7 */ nullptr,
    /* F8 */ nullptr,      /* F9 */ nullptr,      /* FA */ nullptr,      /* FB */ nullptr,
    /* FC */ nullptr,      /* FD */ nullptr,      /* FE */ nullptr,      /* FF */ nullptr,
};

static const char* s_mouseNames[8] = {
    "Mouse1", "Mouse2", "Mouse3", "Mouse4",
    "Mouse5", "Mouse6", "Mouse7", "Mouse8",
};

// VK codes for mouse buttons 0-7; 0 = not available via this API
static const int s_mouseVKs[8] = {
    VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2, 0, 0, 0
};

static const char* s_padNames[26] = {
    "Pad_DUp",    "Pad_DDown",  "Pad_DLeft",  "Pad_DRight",
    "Pad_Start",  "Pad_Back",   "Pad_LThumb", "Pad_RThumb",
    "Pad_LB",     "Pad_RB",     nullptr,       nullptr,
    "Pad_A",      "Pad_B",      "Pad_X",       "Pad_Y",
    "Pad_LT",     "Pad_RT",
    "Pad_LX-",    "Pad_LX+",
    "Pad_LY-",    "Pad_LY+",
    "Pad_RX-",    "Pad_RX+",
    "Pad_RY-",    "Pad_RY+",
};

float axes[10] = {};

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
        if (padIdx < 26)
            return const_cast<char*>(s_padNames[padIdx]);
        return nullptr;
    }
}

float getAxis(ButtonCode button) {
    if (button >= 528 && button <= 537)
        return axes[button - 528];
    if (button < 768)
        return (buttons[button] & 0x80) ? 1.0f : 0.0f;
    return 0.0f;
}

unsigned char actionStateRaw(ButtonCode button) {
    return (button < 768) ? buttons[button] : 0;
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

    // Gamepad: buttons (bits 0-15) + axes (slots 16-25)
    XINPUT_STATE xs = {};
    if (XInputGetState(0, &xs) == ERROR_SUCCESS) {
        WORD w = xs.Gamepad.wButtons;
        for (int i = 0; i < 16; i++)
            buttons[512 + i] = (w >> i) & 1 ? 0x80 : 0;
        auto pos = [](float v) { return v > 0.0f ? v : 0.0f; };
        axes[0] = xs.Gamepad.bLeftTrigger  / 255.0f;
        axes[1] = xs.Gamepad.bRightTrigger / 255.0f;
        axes[2] = pos(-xs.Gamepad.sThumbLX / 32768.0f);
        axes[3] = pos( xs.Gamepad.sThumbLX / 32767.0f);
        axes[4] = pos(-xs.Gamepad.sThumbLY / 32768.0f);
        axes[5] = pos( xs.Gamepad.sThumbLY / 32767.0f);
        axes[6] = pos(-xs.Gamepad.sThumbRX / 32768.0f);
        axes[7] = pos( xs.Gamepad.sThumbRX / 32767.0f);
        axes[8] = pos(-xs.Gamepad.sThumbRY / 32768.0f);
        axes[9] = pos( xs.Gamepad.sThumbRY / 32767.0f);
        for (int i = 0; i < 10; i++)
            buttons[528 + i] = (axes[i] >= SPARK_AXIS_PRESS_THRESHOLD) ? 0x80 : 0;
    } else {
        memset(&buttons[512], 0, 26);
        memset(axes, 0, sizeof(axes));
    }
}

} // namespace Spark::Input
