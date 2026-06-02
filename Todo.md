- Code quality:
    x Break down UI.cpp monolith. Audit similar large files.
    - Move all global state into a struct for cleaner reinitilization on halo1.dll reload.
    - [Blocker] Fix file naming inconsistencies (mostly casing).
    - Break up Entity type by type. Eg: Vehicle, Biped, Weapon, etc.
    - Create a fork of LibSM64 that exposes bone transforms.

    x Engine/mod seperation:
        x Move all hooking into the Halo1 namespace. Engine knowledge should not live in mod directories.
        x All mod logic in hooks should be implemented as middleware.

- Bugs and Stability:
    x Implement level geometry chunking. Keep Mario's coordinates near the origin to avoid overflow.
    - Audit all raw memory accesses. Replace with safe wrappers in non-performance critical code.
    x Find out why updateAllEntities is not running in the underground section of Attack on the Control Room.
    x Fix Mario(.weapon) being available for pickup.
    - [Blocker] Investigate intro cinematic bugs and crashes:
        - Crash: Segfault (eg Silent Cartographer)
        - Bug: Mario in control during intro ride. (eg Silent Cartographer)
    - [Blocker] Investitate Mario randomly teleporting when walking on sloped surfaces (especially dynamic objects).
        - Try to root cause.
        - Implement better teleport guards:
            - Track last safe position.
            - Rollback on long distance teleport.
            - Rollback if no update tick is recieved for N ticks (OOB indicator).
    - [Blocker] Investigate spurious level geometry collision on first elevator ride on 343 Guilty Spark.

- Testing:
    - [Blocker] Do a full playthrough on the release build.

- Gameplay:
    - Movement:
        x Implement collision with vehicles, bipeds and other moving objects.
        - Fix wall kicks for near-vertical walls.
            - Place a vertical wall between Mario and a wall when he is airborne and close.
        - Implement ridable jackal shield. (rideable as koopa shell)
        - Collision:
            - Entity collision:
                - [Blocker] Support collision with other bones besides 0th bone.
                - Support collision with bipeds.
                    - Allow bi-directional pushing. (might need custom collision/solving rather than using Mario engine)
                - Support per tag type model override for finer developer control.
            - Level collision:
                - Level tweak tooling.
                    - Allow placement of planes, boxes, capsules.
                    - Allow deletion of unwanted faces from Halo's BSP.
                    - Level tweak exporter.
        
    - Combat:
        x Implement damage knockback on Mario.
        - Melee
            x Implement melee and shell damage interactions.
            - Customized melee hurtboxes per unit type.
            - Align hurtboxes with unit's orientation (use bone orientation or Entity::fwd/up)
            - Implement less naive rules for when a hand/foot hitbox is live. Should be based off of Mario's animation state.
            - Try out relative speed scaled melee damage.

    x Implement slow motion.
    x Shrink Chief to Mario's size so AI targetting looks correct.
    x Fall damage:

- Presentation:
    x Play sounds from LibSM64.

    - Animation:
        - Inverse kinematics:
            - Arm IK:
                - Smooth out arm IK transition from busy to not busy.
                - Implement pole targets so Mario's elbows bend naturally.
            - Torso IK:
                - Try out making Mario lean slightly as he aims up and down.
        - Head:
            - Bias Mario's look direction towards the direction the camera is facing.
            - Lean Mario's head sideways towards weapon when he is firing/ADSing.
        - Look into blending Mario's animation with Chief's.
        - Drive Mario pose from Chief's in cutscenes and vehicles.
    
    - Third person camera:
        x Disable or move 1rst person weapon flares and muzzle flashes.
        - Render a false projectile exiting the player's barrel.
            - False projectile should converge with the actual projectile over distance.
        - [Blocker] Move or disable all FPV weapon lights/effects.
            
    x Address Mario jitter during pause.

- Usability:
    - Keybinds:
        - [Blocker] Implement remappable keybinds.
        - [Blocker] Reverse MCC's keybinds so we can hook into those too.
        - [Blocker] Address some keys being detected when window is not focused.

- Tooling
    - Cheat Engine:
        x Implement external tag browser in Cheat Engine.
        x Implement external entity browser.
    - Track tags and assets in git.
        - Store diffs relative to vanilla tag values.
        - Export tag data to XML for human readable diffs.
