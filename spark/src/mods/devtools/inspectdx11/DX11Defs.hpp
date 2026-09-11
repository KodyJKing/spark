#pragma once

#include <vector>

namespace Mod::DevTools::InspectDX11 {

    // Vtable slot indices for ID3D11Device, in declaration order per d3d11.h
    // (Windows 10 SDK 10.0.22621.0). ID3D11Device inherits directly from IUnknown.
    // X-macro list drives both the enum and its label table so they can't drift apart.
    #define DX11_DEVICE_VFUNCS(X) \
        X(QueryInterface) \
        X(AddRef) \
        X(Release) \
        X(CreateBuffer) \
        X(CreateTexture1D) \
        X(CreateTexture2D) \
        X(CreateTexture3D) \
        X(CreateShaderResourceView) \
        X(CreateUnorderedAccessView) \
        X(CreateRenderTargetView) \
        X(CreateDepthStencilView) \
        X(CreateInputLayout) \
        X(CreateVertexShader) \
        X(CreateGeometryShader) \
        X(CreateGeometryShaderWithStreamOutput) \
        X(CreatePixelShader) \
        X(CreateHullShader) \
        X(CreateDomainShader) \
        X(CreateComputeShader) \
        X(CreateClassLinkage) \
        X(CreateBlendState) \
        X(CreateDepthStencilState) \
        X(CreateRasterizerState) \
        X(CreateSamplerState) \
        X(CreateQuery) \
        X(CreatePredicate) \
        X(CreateCounter) \
        X(CreateDeferredContext) \
        X(OpenSharedResource) \
        X(CheckFormatSupport) \
        X(CheckMultisampleQualityLevels) \
        X(CheckCounterInfo) \
        X(CheckCounter) \
        X(CheckFeatureSupport) \
        X(GetPrivateData) \
        X(SetPrivateData) \
        X(SetPrivateDataInterface) \
        X(GetFeatureLevel) \
        X(GetCreationFlags) \
        X(GetDeviceRemovedReason) \
        X(GetImmediateContext) \
        X(SetExceptionMode) \
        X(GetExceptionMode)

    enum class ID3D11Device_VirtualFunctions {
        #define X(name) name,
        DX11_DEVICE_VFUNCS(X)
        #undef X
    };

    inline const std::vector<const char*> ID3D11Device_VirtualFunctionNames = {
        #define X(name) #name,
        DX11_DEVICE_VFUNCS(X)
        #undef X
    };

    // Vtable slot indices for ID3D11DeviceContext, in declaration order per d3d11.h
    // (Windows 10 SDK 10.0.22621.0). IUnknown/ID3D11DeviceChild methods included since
    // they occupy the first slots of the same flat vtable.
    #define DX11_DEVICECONTEXT_VFUNCS(X) \
        X(QueryInterface) \
        X(AddRef) \
        X(Release) \
        X(GetDevice) \
        X(GetPrivateData) \
        X(SetPrivateData) \
        X(SetPrivateDataInterface) \
        X(VSSetConstantBuffers) \
        X(PSSetShaderResources) \
        X(PSSetShader) \
        X(PSSetSamplers) \
        X(VSSetShader) \
        X(DrawIndexed) \
        X(Draw) \
        X(Map) \
        X(Unmap) \
        X(PSSetConstantBuffers) \
        X(IASetInputLayout) \
        X(IASetVertexBuffers) \
        X(IASetIndexBuffer) \
        X(DrawIndexedInstanced) \
        X(DrawInstanced) \
        X(GSSetConstantBuffers) \
        X(GSSetShader) \
        X(IASetPrimitiveTopology) \
        X(VSSetShaderResources) \
        X(VSSetSamplers) \
        X(Begin) \
        X(End) \
        X(GetData) \
        X(SetPredication) \
        X(GSSetShaderResources) \
        X(GSSetSamplers) \
        X(OMSetRenderTargets) \
        X(OMSetRenderTargetsAndUnorderedAccessViews) \
        X(OMSetBlendState) \
        X(OMSetDepthStencilState) \
        X(SOSetTargets) \
        X(DrawAuto) \
        X(DrawIndexedInstancedIndirect) \
        X(DrawInstancedIndirect) \
        X(Dispatch) \
        X(DispatchIndirect) \
        X(RSSetState) \
        X(RSSetViewports) \
        X(RSSetScissorRects) \
        X(CopySubresourceRegion) \
        X(CopyResource) \
        X(UpdateSubresource) \
        X(CopyStructureCount) \
        X(ClearRenderTargetView) \
        X(ClearUnorderedAccessViewUint) \
        X(ClearUnorderedAccessViewFloat) \
        X(ClearDepthStencilView) \
        X(GenerateMips) \
        X(SetResourceMinLOD) \
        X(GetResourceMinLOD) \
        X(ResolveSubresource) \
        X(ExecuteCommandList) \
        X(HSSetShaderResources) \
        X(HSSetShader) \
        X(HSSetSamplers) \
        X(HSSetConstantBuffers) \
        X(DSSetShaderResources) \
        X(DSSetShader) \
        X(DSSetSamplers) \
        X(DSSetConstantBuffers) \
        X(CSSetShaderResources) \
        X(CSSetUnorderedAccessViews) \
        X(CSSetShader) \
        X(CSSetSamplers) \
        X(CSSetConstantBuffers) \
        X(VSGetConstantBuffers) \
        X(PSGetShaderResources) \
        X(PSGetShader) \
        X(PSGetSamplers) \
        X(VSGetShader) \
        X(PSGetConstantBuffers) \
        X(IAGetInputLayout) \
        X(IAGetVertexBuffers) \
        X(IAGetIndexBuffer) \
        X(GSGetConstantBuffers) \
        X(GSGetShader) \
        X(IAGetPrimitiveTopology) \
        X(VSGetShaderResources) \
        X(VSGetSamplers) \
        X(GetPredication) \
        X(GSGetShaderResources) \
        X(GSGetSamplers) \
        X(OMGetRenderTargets) \
        X(OMGetRenderTargetsAndUnorderedAccessViews) \
        X(OMGetBlendState) \
        X(OMGetDepthStencilState) \
        X(SOGetTargets) \
        X(RSGetState) \
        X(RSGetViewports) \
        X(RSGetScissorRects) \
        X(HSGetShaderResources) \
        X(HSGetShader) \
        X(HSGetSamplers) \
        X(HSGetConstantBuffers) \
        X(DSGetShaderResources) \
        X(DSGetShader) \
        X(DSGetSamplers) \
        X(DSGetConstantBuffers) \
        X(CSGetShaderResources) \
        X(CSGetUnorderedAccessViews) \
        X(CSGetShader) \
        X(CSGetSamplers) \
        X(CSGetConstantBuffers) \
        X(ClearState) \
        X(Flush) \
        X(GetType) \
        X(GetContextFlags) \
        X(FinishCommandList)

    enum class ID3D11DeviceContext_VirtualFunctions {
        #define X(name) name,
        DX11_DEVICECONTEXT_VFUNCS(X)
        #undef X
    };

    inline const std::vector<const char*> ID3D11DeviceContext_VirtualFunctionNames = {
        #define X(name) #name,
        DX11_DEVICECONTEXT_VFUNCS(X)
        #undef X
    };

}