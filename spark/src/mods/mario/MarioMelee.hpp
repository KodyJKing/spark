#pragma once

namespace Mod::Mario::MarioMelee {
    // Called once per game tick (from Mario::update(), after updateMarioPose).
    void tick();

    // Called each debug-render frame to draw fist spheres and nearby target capsules.
    void debugRender();
}
