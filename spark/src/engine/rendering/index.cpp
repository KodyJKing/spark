
#include "../common.hpp"
#include <d3d11.h>

namespace Engine {
    ID3D11DeviceContext* getD3D11Context() {
        return *reinterpret_cast<ID3D11DeviceContext**>(dllBase() + 0x2EA2D30);
    }

    ID3D11Device* getD3D11Device() {
        auto ctx = getD3D11Context();
        ID3D11Device* device;
        ctx->GetDevice(&device);
        return device;
    }
}
