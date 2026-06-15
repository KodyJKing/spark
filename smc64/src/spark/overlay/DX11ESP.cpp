#include "ESP.hpp"

#include "engine/rendering/index.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3dcompiler")

/**
 * Native DX11 line drawing in world space, without going through ImGui.
 *
 * World-space vertices are transformed by a view-projection constant buffer
 * and depth-tested against Halo's reversed depth buffer (GREATER_EQUAL, no
 * depth write) for correct occlusion against scene geometry.
 */
namespace Spark::Overlay::ESP::DX11 {

    // World-space vertices are transformed by the view-projection matrix in the constant buffer.
    const char* vertexShaderSource = R"(
        cbuffer CameraTransform : register(b0) {
            float4x4 viewProj;
        };
        struct VS_INPUT {
            float3 Pos : POSITION;
            float4 Color : COLOR;
        };
        struct PS_INPUT {
            float4 Pos : SV_POSITION;
            float4 Color : COLOR;
        };
        PS_INPUT main(VS_INPUT input) {
            PS_INPUT output;
            output.Pos = mul(viewProj, float4(input.Pos, 1.0));
            output.Color = input.Color;
            return output;
        }
    )";
    const char* pixelShaderSource = R"(
        struct PS_INPUT {
            float4 Pos : SV_POSITION;
            float4 Color : COLOR;
        };
        float4 main(PS_INPUT input) : SV_TARGET {
            return input.Color;
        }
    )";

    struct Vertex {
        float   x, y, z;
        uint8_t r, g, b, a;
    };

    static constexpr UINT MAX_VERTS = 2048; // 1024 lines

    static ID3D11VertexShader*      s_vertexShader    = nullptr;
    static ID3D11PixelShader*       s_pixelShader     = nullptr;
    static ID3D11InputLayout*       s_inputLayout     = nullptr;
    static ID3D11Buffer*            s_vertexBuffer    = nullptr;
    static ID3D11Buffer*            s_constantBuffer  = nullptr;
    static ID3D11BlendState*        s_blendState      = nullptr;
    static ID3D11RasterizerState*   s_rasterizerState = nullptr;
    static ID3D11DepthStencilState* s_depthState      = nullptr;

    static Vertex s_drawList[MAX_VERTS];
    static UINT   s_vertexCount       = 0;
    static float  s_depthBias         = 0.0f;
    static float  s_cachedDepthBias   = 0.0f; // matches bias baked into s_rasterizerState

    void init() {
        ID3D11DeviceContext* context = Engine::getD3D11Context();
        if (!context) return;

        ID3D11Device* device = nullptr;
        context->GetDevice(&device);
        if (!device) return;

        // --- Compile shaders ---
        ID3DBlob* vsBlob    = nullptr;
        ID3DBlob* psBlob    = nullptr;
        ID3DBlob* errorBlob = nullptr;

        bool ok = SUCCEEDED(D3DCompile(
            vertexShaderSource, strlen(vertexShaderSource),
            nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob));
        if (errorBlob) { errorBlob->Release(); errorBlob = nullptr; }

        ok = ok && SUCCEEDED(D3DCompile(
            pixelShaderSource, strlen(pixelShaderSource),
            nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob));
        if (errorBlob) { errorBlob->Release(); }

        if (!ok) {
            if (vsBlob) vsBlob->Release();
            if (psBlob) psBlob->Release();
            device->Release();
            return;
        }

        device->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vertexShader);
        device->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_pixelShader);

        // --- Input layout ---
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(Vertex, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        device->CreateInputLayout(
            layout, 2,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            &s_inputLayout);

        vsBlob->Release();
        psBlob->Release();

        // --- Dynamic vertex buffer (2 vertices per drawLine call) ---
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth      = sizeof(Vertex) * MAX_VERTS;
        vbDesc.Usage          = D3D11_USAGE_DYNAMIC;
        vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&vbDesc, nullptr, &s_vertexBuffer);

        // --- Alpha blend state ---
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable           = TRUE;
        blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&blendDesc, &s_blendState);

        // --- No-cull solid rasterizer (no scissor) ---
        D3D11_RASTERIZER_DESC rastDesc = {};
        rastDesc.FillMode        = D3D11_FILL_SOLID;
        rastDesc.CullMode        = D3D11_CULL_NONE;
        rastDesc.DepthClipEnable = TRUE;
        device->CreateRasterizerState(&rastDesc, &s_rasterizerState);

        // --- Read Halo's reversed depth buffer; never write to it ---
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable    = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc      = D3D11_COMPARISON_GREATER_EQUAL;
        device->CreateDepthStencilState(&dsDesc, &s_depthState);

        // --- View-projection constant buffer ---
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth      = sizeof(float) * 16;
        cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&cbDesc, nullptr, &s_constantBuffer);

        device->Release();
    }

    void free() {
        auto safeRelease = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
        safeRelease(s_depthState);
        safeRelease(s_rasterizerState);
        safeRelease(s_blendState);
        safeRelease(s_constantBuffer);
        safeRelease(s_vertexBuffer);
        safeRelease(s_inputLayout);
        safeRelease(s_pixelShader);
        safeRelease(s_vertexShader);
    }

    static constexpr float CLIP_NEAR = 0.00782943f;
    static constexpr float CLIP_FAR  = 386.369f;

    struct SavedState {
        ID3D11VertexShader*      vs;      UINT vsN; ID3D11ClassInstance* vsI[4];
        ID3D11PixelShader*       ps;      UINT psN; ID3D11ClassInstance* psI[4];
        ID3D11InputLayout*       il;
        D3D11_PRIMITIVE_TOPOLOGY top;
        ID3D11Buffer*            vb;      UINT stride, offset;
        ID3D11BlendState*        bs;      float bFactor[4]; UINT bSample;
        ID3D11RasterizerState*   rs;
        ID3D11DepthStencilState* ds;      UINT stencilRef;
        ID3D11Buffer*            cb0;
    };
    static SavedState s_saved;

    bool cameraIsValid() {
        auto& cam = ESP::camera;
        return !(cam.fwd.x == 0.0f && cam.fwd.y == 0.0f && cam.fwd.z == 0.0f);
    }

    void begin() {
        if (!s_vertexBuffer) return;

        s_vertexCount = 0;
        s_depthBias   = 0.0f;

        ESP::updateScreenSize();
        ESP::updateCamera();
        auto& cam = ESP::camera;

        // Guard against uninitialized camera (fwd == zero crashes XMMatrixLookToRH).
        if (!cameraIsValid()) return;

        ID3D11DeviceContext* context = Engine::getD3D11Context();

        // --- Save pipeline state ---
        s_saved = {};
        s_saved.vsN = s_saved.psN = 4;
        context->VSGetShader(&s_saved.vs, s_saved.vsI, &s_saved.vsN);
        context->PSGetShader(&s_saved.ps, s_saved.psI, &s_saved.psN);
        context->IAGetInputLayout(&s_saved.il);
        context->IAGetPrimitiveTopology(&s_saved.top);
        context->IAGetVertexBuffers(0, 1, &s_saved.vb, &s_saved.stride, &s_saved.offset);
        context->OMGetBlendState(&s_saved.bs, s_saved.bFactor, &s_saved.bSample);
        context->RSGetState(&s_saved.rs);
        context->OMGetDepthStencilState(&s_saved.ds, &s_saved.stencilRef);
        context->VSGetConstantBuffers(0, 1, &s_saved.cb0);

        // --- Build and upload view-projection matrix ---
        // Halo is Z-up. Near/far are swapped to match Halo's reversed depth buffer.
        using namespace DirectX;
        float fovY = cam.verticalFov
            ? cam.fov
            : 2.0f * atanf((cam.height / cam.width) * tanf(cam.fov / 2.0f));
        XMVECTOR camPos = XMVectorSet(cam.pos.x, cam.pos.y, cam.pos.z, 0.0f);
        XMVECTOR camFwd = XMVectorSet(cam.fwd.x, cam.fwd.y, cam.fwd.z, 0.0f);
        XMMATRIX view     = XMMatrixLookToRH(camPos, camFwd, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        XMMATRIX proj     = XMMatrixPerspectiveFovRH(fovY, cam.width / cam.height, CLIP_FAR, CLIP_NEAR);
        XMMATRIX viewProj = view * proj;

        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context->Map(s_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &viewProj, sizeof(XMMATRIX));
            context->Unmap(s_constantBuffer, 0);
        }

        // --- Set our pipeline state ---
        // Rebuild rasterizer state if depth bias changed since last frame.
        if (s_depthBias != s_cachedDepthBias) {
            ID3D11Device* device = nullptr;
            context->GetDevice(&device);
            if (device) {
                if (s_rasterizerState) { s_rasterizerState->Release(); s_rasterizerState = nullptr; }
                D3D11_RASTERIZER_DESC rastDesc = {};
                rastDesc.FillMode             = D3D11_FILL_SOLID;
                rastDesc.CullMode             = D3D11_CULL_NONE;
                rastDesc.DepthClipEnable      = TRUE;
                rastDesc.SlopeScaledDepthBias = s_depthBias;
                device->CreateRasterizerState(&rastDesc, &s_rasterizerState);
                device->Release();
                s_cachedDepthBias = s_depthBias;
            }
        }
        UINT stride = sizeof(Vertex), offset = 0;
        context->IASetInputLayout(s_inputLayout);
        context->IASetVertexBuffers(0, 1, &s_vertexBuffer, &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        context->VSSetShader(s_vertexShader, nullptr, 0);
        context->PSSetShader(s_pixelShader, nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &s_constantBuffer);
        context->OMSetBlendState(s_blendState, nullptr, 0xFFFFFFFF);
        context->RSSetState(s_rasterizerState);
        context->OMSetDepthStencilState(s_depthState, 0);
    }

    void end() {
        if (!cameraIsValid()) return;

        ID3D11DeviceContext* context = Engine::getD3D11Context();

        if (s_vertexCount > 0) {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(context->Map(s_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, s_drawList, sizeof(Vertex) * s_vertexCount);
                context->Unmap(s_vertexBuffer, 0);
                context->Draw(s_vertexCount, 0);
            }
        }

        context->VSSetShader(s_saved.vs, s_saved.vsI, s_saved.vsN);
        context->PSSetShader(s_saved.ps, s_saved.psI, s_saved.psN);
        context->IASetInputLayout(s_saved.il);
        context->IASetPrimitiveTopology(s_saved.top);
        context->IASetVertexBuffers(0, 1, &s_saved.vb, &s_saved.stride, &s_saved.offset);
        context->OMSetBlendState(s_saved.bs, s_saved.bFactor, s_saved.bSample);
        context->RSSetState(s_saved.rs);
        context->OMSetDepthStencilState(s_saved.ds, s_saved.stencilRef);
        context->VSSetConstantBuffers(0, 1, &s_saved.cb0);

        // GetXxx calls in begin() bumped ref counts; release them now
        auto safeRelease = [](auto* p) { if (p) p->Release(); };
        safeRelease(s_saved.vs);
        for (UINT i = 0; i < s_saved.vsN; i++) safeRelease(s_saved.vsI[i]);
        safeRelease(s_saved.ps);
        for (UINT i = 0; i < s_saved.psN; i++) safeRelease(s_saved.psI[i]);
        safeRelease(s_saved.il);
        safeRelease(s_saved.vb);
        safeRelease(s_saved.bs);
        safeRelease(s_saved.rs);
        safeRelease(s_saved.ds);
        safeRelease(s_saved.cb0);
    }

    void setDepthBias(float bias) {
        s_depthBias = bias;
    }

    void drawLine(Vec3 a, Vec3 b, ImU32 color) {
        if (s_vertexCount + 2 > MAX_VERTS) return;

        auto& cam = ESP::camera;

        // --- CPU near-plane clip: prevents degenerate vertices behind the camera ---
        {
            Vec3  n    = cam.fwd;
            float dist = cam.pos.dot(n) + CLIP_NEAR;
            float dA   = a.dot(n) - dist;
            float dB   = b.dot(n) - dist;
            if (dA < 0 && dB < 0) return;
            if (dA < 0) a = a + (b - a) * (dA / (dA - dB));
            if (dB < 0) b = b + (a - b) * (dB / (dB - dA));
        }

        // ImU32 is packed ABGR (little-endian): R=byte0, G=byte1, B=byte2, A=byte3
        uint8_t r  = (color >>  0) & 0xFF;
        uint8_t g  = (color >>  8) & 0xFF;
        uint8_t bv = (color >> 16) & 0xFF;
        uint8_t av = (color >> 24) & 0xFF;

        s_drawList[s_vertexCount++] = { a.x, a.y, a.z, r, g, bv, av };
        s_drawList[s_vertexCount++] = { b.x, b.y, b.z, r, g, bv, av };
    }

}
