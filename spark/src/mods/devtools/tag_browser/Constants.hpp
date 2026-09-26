#pragma once

#include "utils/Strings.hpp"
#include <cstdint>

namespace Mod::DevTools {

    struct GroupID {
        const char* name;
        uint32_t groupID;
    };

    #define GROUP_ID_ALL 0

    GroupID ids[] = {
        {"All", GROUP_ID_ALL},
        #define GROUP_ID(friendlyName, fourccStr) {friendlyName, Strings::stringToFourcc(fourccStr)},
        GROUP_ID("Weapon", "weap")
        GROUP_ID("Projectile", "proj")
        GROUP_ID("Damage", "jpt!")
        GROUP_ID("Vehicle", "vehi")
        GROUP_ID("Biped", "bipd")
        GROUP_ID("Scenenery", "scen")
        GROUP_ID("Device", "devi")
        GROUP_ID("Equipment", "eqip")
        GROUP_ID("Effect", "effe")
        GROUP_ID("Contrail", "cont")
        GROUP_ID("Particle", "part")
        GROUP_ID("Sound", "snd!")
        GROUP_ID("Animation", "antr")
        GROUP_ID("Actor", "actr")
        GROUP_ID("Actor Variant", "actv")
        GROUP_ID("Bitmap", "bitm")
        GROUP_ID("Shader", "shdr")
        GROUP_ID("Light", "ligh")
        GROUP_ID("BSP", "sbsp")
        GROUP_ID("Collision Model", "coll")
        GROUP_ID("Model", "mod2")
        #undef GROUP_ID
    };

    #define NUM_GROUP_IDS (sizeof(ids) / sizeof(GroupID))

}
