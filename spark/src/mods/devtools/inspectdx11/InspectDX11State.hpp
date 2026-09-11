#pragma once

#include "DX11Defs.hpp"
#include "engine/rendering/index.hpp"
#include <d3d11.h>
#include <set>

#include "MinHook.h"

namespace Mod::DevTools::InspectDX11 {
    class State {
        public:
        static inline std::set<ID3D11Resource*> resources;
        static inline std::set<ID3D11Buffer*> buffers;

        static inline bool collectingMappedResources;
        static inline void setCollectingMappedResources(bool collecting) {
            collectingMappedResources = collecting;
            hookContext(collecting, ID3D11DeviceContext_VirtualFunctions::Map, &onMap, (void**)&originalMapFunction);
        }

        private:
        static inline HRESULT (*originalMapFunction)(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource, D3D11_MAP mapType, UINT mapFlags, D3D11_MAPPED_SUBRESOURCE* mappedResource);
        static inline HRESULT onMap(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource, D3D11_MAP mapType, UINT mapFlags, D3D11_MAPPED_SUBRESOURCE* mappedResource) {
            if (collectingMappedResources) {
                resources.insert(resource);
            }
            return originalMapFunction(context, resource, subresource, mapType, mapFlags, mappedResource);
        }

        static inline bool hookDevice(bool install, ID3D11Device_VirtualFunctions device, void* newFunction, void** originalFunction) {
            ID3D11DeviceContext* deviceContext = Engine::getD3D11Context();
            if (!deviceContext) return false;
            ID3D11Device* d3dDevice;
            deviceContext->GetDevice(&d3dDevice);
            if (!d3dDevice) return false;
            void** vftable = *reinterpret_cast<void***>(d3dDevice);
            void* function = vftable[(size_t)device];
            if (install) {
                MH_CreateHook(function, newFunction, originalFunction);
                MH_EnableHook(function);
            } else {
                MH_DisableHook(function);
                MH_RemoveHook(function);
            }
            return true;
        }

        static inline bool hookContext(bool install, ID3D11DeviceContext_VirtualFunctions context, void* newFunction, void** originalFunction) {
            ID3D11DeviceContext* deviceContext = Engine::getD3D11Context();
            if (!deviceContext) return false;
            void** vftable = *reinterpret_cast<void***>(deviceContext);
            void* function = vftable[(size_t)context];
            if (install) {
                MH_CreateHook(function, newFunction, originalFunction);
                MH_EnableHook(function);
            } else {
                MH_DisableHook(function);
                MH_RemoveHook(function);
            }
            return true;
        }
    };
}
