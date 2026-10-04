#pragma once
#include <d3d11.h>
#include "model_data.hpp"
#include "vertex_buffer_slice.hpp"
#include "index_buffer_slice.hpp"
#include "buffer_source.hpp"

namespace Engine {
    ID3D11DeviceContext* getD3D11Context();
    ID3D11Device* getD3D11Device();
}
