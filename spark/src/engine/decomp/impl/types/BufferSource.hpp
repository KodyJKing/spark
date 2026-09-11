#include <d3d11.h>

namespace Engine::Decomp {

    // Ghidra (_drawModel disasm): BufferSource::object is a pointer to a polymorphic object -
    // the call is a real virtual dispatch (this=object, vtable slot 2 / offset 0x10), not a
    // raw function-pointer call on BufferSource itself. Slots 0/1 are unidentified.
    struct IBufferSourceObject {
        virtual void unknown0() = 0;
        virtual void unknown1() = 0;
        virtual ID3D11Buffer* getBuffer() = 0;
    };

    struct BufferSource {
        IBufferSourceObject* object;
    };

}
