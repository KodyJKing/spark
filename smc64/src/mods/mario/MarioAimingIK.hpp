#pragma once

namespace HaloCE::Mod::Mario::MarioAimingIK {
    // Returns true when Mario's arms are occupied (attacking, facing away, etc.).
    // Used by MarioModel::updateWeaponPose to determine orientation copying.
    bool marioArmsBusy();

    // Called once per game tick after updateMarioPose(), before any render-phase
    // entity processing. Reads camera direction, solves arm IK toward the weapon,
    // and writes the result back into marioPose so all downstream systems see a
    // consistent, fully-solved pose.
    void apply();
}
