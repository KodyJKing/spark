#include "tag_data_types.hpp"

namespace Engine::Map {

    struct Sprite {
        char pad0[0x20]; // Wow!
    };
    static_assert(sizeof(Sprite) == 0x20);
    
    struct GroupSequence {
        char pad0[0x34];
        BlockPointerTyped<Sprite> sprites;
    };
    static_assert(sizeof(GroupSequence) == 0x40);

    struct BitmapData {
        uint32_t bitmapClass; // Todo: Enum
        uint16_t width;
        uint16_t height;
        uint16_t depth;
        uint16_t type;
        uint16_t format;
        uint16_t flags;
        int16_t registrationX;
        int16_t registrationY;
        uint16_t mipmapCount;
        uint16_t pad0;
        Pointer<PointerBase_Map> data;
        uint32_t dataSize;
        uint32_t bitmapTagHandle;
        uint32_t textureCacheHandle;
        uint32_t bufferIndex;
        uint32_t pad1;
    };
    static_assert(sizeof(BitmapData) == 0x30);
    static_assert(offsetof(BitmapData, data) == 0x18);
    
    struct Bitmap {
        char pad0[0x54];
        BlockPointerTyped<GroupSequence> groupSequences;
        BlockPointerTyped<BitmapData> bitmapData;
    };
    static_assert(sizeof(Bitmap) == 0x6C);

    using SparkBitmap = Bitmap;
    using SparkBitmapData = BitmapData;

};

