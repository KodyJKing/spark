#pragma once
#include <cstddef>
#include "tag_data_types.hpp"

namespace Engine::Map {

    struct SoundPermutation {
        char name[0x20];
        char pad0[0x3C - 0x20];
        uint32_t soundTagHandle;
        DataPointer samples;
        DataPointer mouthData;
        DataPointer subtitleData;
    };
    static_assert(sizeof(SoundPermutation) == 0x7C);
    static_assert(offsetof(SoundPermutation, soundTagHandle) == 0x3C);
    static_assert(offsetof(SoundPermutation, samples) == 0x40);
    
    struct PitchRange {
        char name[0x20];
        char pad0[0x3C - 0x20];
        BlockPointerTyped<SoundPermutation> permutations;
    };
    static_assert(sizeof(PitchRange) == 0x48);
    static_assert(offsetof(PitchRange, permutations) == 0x3C);

    struct Sound {
        char pad0[0x98];
        BlockPointerTyped<PitchRange> pitchRanges;
    };
    static_assert(sizeof(Sound) == 0xA4);
    static_assert(offsetof(Sound, pitchRanges) == 0x98);

}
