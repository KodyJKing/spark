#include <Windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "Overlay.hpp"
#include "DX11Hook.hpp"
#include "halomcc/HaloMCC.hpp"
#include "Licenses.hpp"
#include "spark/overlay/ESP.hpp"
#include "utils/UnloadLock.hpp"
#include "math/Math.hpp"
#include "version.h"

#include "spark/RenderBuses.hpp"
#include "engine/halo1.hpp"
#include "spark/SparkHost.hpp"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Spark::Overlay {

    HWND gameWindow;

    HWND getGameWindow() {
        return gameWindow;
    }

    void initializeContext(HWND targetWindow) {
        if (ImGui::GetCurrentContext( ))
            return;
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(targetWindow);
    }

    void credits() {
        ImGui::SeparatorText("Credits");
        if ( ImGui::CollapsingHeader("ImGui") ) ImGui::TextWrapped(Spark::Licenses::imGui);
        if ( ImGui::CollapsingHeader("MinHook") ) ImGui::TextWrapped(Spark::Licenses::minHook);
        if ( ImGui::CollapsingHeader("Zydis") ) ImGui::TextWrapped(Spark::Licenses::zydis);
        if ( ImGui::CollapsingHeader("UniversalHookX") ) ImGui::TextWrapped(Spark::Licenses::universalHookX);
        ImGui::TextWrapped("Thanks to Kavawuvi for their documentation of the Halo CE map and tag format.");
    }

    void mainModWindow() {
        ImGui::Begin("Spark");

        // Default window size
        auto winSize = ImVec2(300, 400);
        ImGui::SetWindowSize(winSize, ImGuiCond_Once);
        // Default position to bottom right
        auto displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetWindowPos(ImVec2(displaySize.x - winSize.x - 10, displaySize.y - winSize.y - 10), ImGuiCond_Once);

        // Tabs
        if (ImGui::BeginTabBar("Spark Tabs")) {
            if (ImGui::BeginTabItem("About")) {
                const char* config =
                    #ifdef _DEBUG
                        "Debug";
                    #else
                        "Release";
                    #endif
                ImGui::Text("Spark %s %s", config, SMC64_VERSION_STRING);
                ImGui::Text("Built %s, %s", __DATE__, __TIME__);
                credits();
                ImGui::EndTabItem();
            }
            Spark::onRenderPauseMenuTabs.dispatch(Spark::noopTerminal, nullptr);
            ImGui::EndTabBar();
        }        

        ImGui::End();
    }

    void loadedIndicatorWindow() {
        static uint64_t startTick = GetTickCount64();
        uint64_t now = GetTickCount64();
        float uptime = (now - startTick) / 1000.0f;
        float sin = 0.5f * sinf(uptime * 7.0f) + 0.5f;
        float blink = Math::lerp(0.7f, 1.0f, sin);
        // float fade = Math::smoothstep(9.0f, 10.0f, uptime);
        float alpha = blink; // blink * (1.0f - fade);

        // if ( fade >= 0.99f )
        //     return;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        ImGui::Begin(
            "##SparkIndicator", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground
        );

            // Center window horizontally.
            auto displaySize = ImGui::GetIO().DisplaySize;
            auto windowSize = ImGui::GetWindowSize();
            ImGui::SetWindowPos(ImVec2((displaySize.x - windowSize.x) / 2, 0));

            ImGui::Text("Spark Loaded");
        ImGui::End();

        ImGui::PopStyleVar();
    }

    void render() {
        UnloadLock lock; // Prevent unloading while rendering

        if (SparkHost::bExit)
            return;

        bool paused = HaloMCC::isPauseMenuOpen();

        auto io = ImGui::GetIO();
        io.WantCaptureKeyboard = paused;
        io.WantCaptureMouse = paused;
        io.NavActive = paused;
        if (!paused)
            ShowCursor(FALSE);

        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (!Engine::isGameLoaded())
            loadedIndicatorWindow();

        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            Spark::showDebugOverlay = !Spark::showDebugOverlay;

        if (Spark::showDebugOverlay) {
            // Sync ESP camera to the player camera, then fire the world-space debug render bus.
            // Spark owns this sync; handlers must not touch Overlay::ESP::camera setup.
            if (Engine::isGameLoaded()) {
                const float fovScale = 0.627f; // Magic number to convert Halo's vertical FOV to the horizontal FOV used by our projection math.
                auto haloCam = Engine::getPlayerCameraPointer();
                auto& cam = Overlay::ESP::camera;
                cam.pos = haloCam->pos;
                cam.fwd = haloCam->fwd;
                cam.up  = haloCam->up;
                cam.fov = haloCam->fov * fovScale;
                cam.verticalFov = true;
                Overlay::ESP::beginESPWindow("__ESP");
                Spark::onRenderDebugWorld.dispatch(Spark::noopTerminal, nullptr);
                Overlay::ESP::endESPWindow();
            }

            Spark::onRenderDebugUI.dispatch(Spark::noopTerminal, nullptr);
        }

        if ( paused )
            mainModWindow();

        ImGui::EndFrame();
    }

    namespace WndProcHook {
        // Original WndProc
        WNDPROC originalWndProc = nullptr;

        LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            auto result = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

            bool blockMessage = false;
            switch (msg) {
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_CHAR:
                case WM_SYSCHAR:
                    blockMessage = ImGui::GetIO().WantCaptureKeyboard;
                    break;
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                case WM_MOUSEWHEEL:
                case WM_MOUSEHWHEEL:
                    blockMessage = ImGui::GetIO().WantCaptureMouse;
                    break;
                case WM_INPUT:
                    blockMessage = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
                    break;
            }

            if (blockMessage)
                return result;

            return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
        }

        void hook(HWND targetWindow) {
            originalWndProc = (WNDPROC) SetWindowLongPtr(targetWindow, GWLP_WNDPROC, (LONG_PTR) hkWndProc);
        }

        void unhook(HWND targetWindow) {
            SetWindowLongPtr(targetWindow, GWLP_WNDPROC, (LONG_PTR) originalWndProc);
        }
    }

    void init() {
        gameWindow = DX11Hook::findMainWindow();
        DX11Hook::hook(gameWindow);
        WndProcHook::hook(gameWindow);
        ESP::DX11::init();
    }

    void free() {
        WndProcHook::unhook(gameWindow);
        DX11Hook::unhook();
        ESP::DX11::free();
    }

}
