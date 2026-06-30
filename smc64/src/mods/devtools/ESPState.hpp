#pragma once
#include "engine/halo1.hpp"

namespace Mod::DevTools {

    struct ESPSettings {
        bool anchorHighlight = false;
        float fovScale = 0.627f;
        float maxDistance = 20.0f;
        float maxBSPVertexDistance = 5.0f;
        struct Filter {
            bool biped      = true;
            bool vehicle    = false;
            bool weapon     = false;
            bool projectile = false;
            bool scenery    = false;
            bool equipment  = false;
            bool other      = false;
        } filter = {};
    };

    struct View {
        bool pos       = true;
        bool vel       = false;
        bool fwd       = false;
        bool up        = false;
        bool angles    = false;
        bool health    = false;
        bool shield    = false;
        bool tag       = false;
        bool tagID     = false;
        bool tagCC     = false;
        bool tagPath   = true;
        bool tagCategory = false;
        bool animation = false;
        bool bones     = false;
        bool worldBones  = true;
        bool collision   = true;
        bool renderBsp   = false;
    };

    inline Engine::Entity* highlightEntity = nullptr;
    inline ESPSettings espSettings = {};
    inline View view = {};

    inline bool freezeTime = false;
    inline bool invincibility = false;
    inline bool noDamageToAnyone = false;

} // namespace Mod::DevTools
