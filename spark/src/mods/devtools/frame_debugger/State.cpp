#include "State.hpp"
#include "MinHook.h"
#include <d3d11.h>
#include "../inspectdx11/DX11Defs.hpp"
#include "spark/overlay/DX11Hook.hpp"
#include "engine/rendering/index.hpp"
#include "utils/Debugging.hpp"
#include <intrin.h>

namespace Mod::DevTools::FrameDebugger {

    using namespace InspectDX11;

    bool isEnabled = false;

    typedef void(__stdcall* DrawIndexedFn)(
        ID3D11DeviceContext* context,
        UINT indexCount,
        UINT startIndexLocation,
        INT baseVertexLocation
    );
    DrawIndexedFn originalDrawIndexed = nullptr;

    bool blinkFrame() {
        return state.frameCount & 1;
    }

    void hkDrawIndexed(
        ID3D11DeviceContext* context,
        UINT indexCount,
        UINT startIndexLocation,
        INT baseVertexLocation
    ) {
        bool applicable = !Spark::Overlay::DX11Hook::isRenderingImGui() && state.enabled();
        if (!applicable) return originalDrawIndexed(context, indexCount, startIndexLocation, baseVertexLocation);
        state.drawsThisFrame++;
        uint64_t stopAt = state.stopAtCall();
        if (state.drawsThisFrame == stopAt) {
            state.finalCallReturnAddress = _ReturnAddress();
            if (state.debug) {
                state.debug = false;
                Debugging::debugMe();
            }
            if (blinkFrame()) return;
        }
        if (state.drawsThisFrame > stopAt) return;
        originalDrawIndexed(context, indexCount, startIndexLocation, baseVertexLocation);
    }

    void** getVTable() {
        auto ctx = Engine::getD3D11Context();
        if (!ctx) return nullptr;
        return *reinterpret_cast<void***>(ctx);
    }

    DrawIndexedFn getDrawIndexed() {
        void** vtable = getVTable();
        if (!vtable) return nullptr;
        size_t index = (size_t) ID3D11DeviceContext_VirtualFunctions::DrawIndexed;
        return reinterpret_cast<DrawIndexedFn>(vtable[index]);
    }

    bool addHooks() {
        DrawIndexedFn drawIndexed = getDrawIndexed();
        if (!drawIndexed) return false;
        MH_CreateHook(drawIndexed, &hkDrawIndexed, reinterpret_cast<void**>(&originalDrawIndexed));
        MH_EnableHook(drawIndexed);
        return true;
    }

    void removeHooks() {
        DrawIndexedFn drawIndexed = getDrawIndexed();
        if (!drawIndexed) return;
        MH_DisableHook(drawIndexed);
        MH_RemoveHook(drawIndexed);
    }
    
    ////////////////////////////////////////////////

    uint64_t State::stopAtCall() {
        return drawsToAllow;
    }

    bool State::enabled() {
        return isEnabled;
    }

    void State::setEnabled(bool value) {
        if (value == isEnabled) {
            return;
        }
        isEnabled = value;
        if (isEnabled) {
            addHooks();
        } else {
            removeHooks();
        }
    }

    void State::deinit() {
        setEnabled(false);
    }

}
