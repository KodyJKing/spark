
#include "../common.hpp"
#include <d3d11.h>

namespace Engine {
    ID3D11DeviceContext* getD3D11Context() {
        return *reinterpret_cast<ID3D11DeviceContext**>(dllBase() + 0x2EA2D30);
    }
}
