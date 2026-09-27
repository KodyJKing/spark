#pragma once

#include "utils/Strings.hpp"

namespace Engine {

    enum TagGroupId {
        #define ENTRY(name, value) GroupId_##name = FOUR_CC_STR(#value)
        ENTRY(Weapon, weap),
        ENTRY(Biped, bipd),
        ENTRY(Character, chr),
        ENTRY(Scenario, scen),
        ENTRY(Vehicle, veh),
        ENTRY(Effect, effe),
        ENTRY(Joint, jpt!),
        ENTRY(Material, matg),
        ENTRY(Physics, phy!),
        ENTRY(Projectile, proj),
        ENTRY(Damage, jpt!),
        ENTRY(Device, devi),
        ENTRY(Equipment, eqip),
        ENTRY(Contrail, cont),
        ENTRY(Particle, part),
        ENTRY(Sound, snd!),
        ENTRY(Animation, antr),
        ENTRY(Actor, actr),
        ENTRY(ActorVariant, actv),
        ENTRY(Bitmap, bitm),
        ENTRY(Shader, shdr),
        ENTRY(Light, ligh),
        ENTRY(BSP, sbsp),
        ENTRY(CollisionModel, coll),
        #undef ENTRY
        GroupId_Invalid = 0xFFFFFF
    };

}