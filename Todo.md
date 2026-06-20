- Code quality:
    - Move all global state into a struct for cleaner reinitilization on halo1.dll reload.
    - [Blocker] Fix file naming inconsistencies (mostly casing).
    - Break up Entity type by type. Eg: Vehicle, Biped, Weapon, etc.
    - Create a fork of LibSM64 that exposes bone transforms.
    - [Major] Fix `SpawnProjectile` misnomer (should be `SpawnObject`).
    - [Major] Fix namespace naming inconsistencies in Mario mod. (should match DevTools mod)
    - [Major] Rename Entity to Object to agree with Halo's naming conventions.
        - Source
        - Ghidra
        - Cheat Engine
        - Documentation

- Bugs and Stability:
    - Audit all raw memory accesses. Replace with safe wrappers in non-performance critical code.
    - [Blocker] Fix ghost entity colliders getting left behind.
    - [Blocker] Fix Mario getting stuck in hallway panels and columns on Covenant ship levels.
        - Truth and Reconciliation
        - Keyes
    - [Blocker] Fix Mario falling through lifts:
        - The Library
        - The Maw

- Performance:
    - [Major] Optimize BSP loading transitions.

- Testing:
    - [Blocker] Do a full playthrough on the release build.
    - [Blocker] Test every map for problem triangles and delete them.
        - Pay special attention to Covenant hallways and columns.

- Gameplay:
    - Movement:
        - Fix wall kicks for near-vertical walls.
            - Place a vertical wall between Mario and a wall when he is airborne and close.
        - Implement ridable jackal shield. (rideable as koopa shell)
        - Collision:
            - Entity collision:
                - Support collision with bipeds.
                    - Allow bi-directional pushing. (might need custom collision/solving rather than using Mario engine)
                - Support per tag type model override for finer developer control.
            - Level collision:
                - Level tweak tooling.
                    - Allow placement of planes, boxes, capsules.
                    - Allow deletion of unwanted faces from Halo's BSP.
                    - Level tweak exporter.
        
    - Combat:
        - Melee
            x Implement melee and shell damage interactions.
            - Customized melee hurtboxes per unit type.
            - Align hurtboxes with unit's orientation (use bone orientation or Entity::fwd/up)
            - Implement less naive rules for when a hand/foot hitbox is live. Should be based off of Mario's animation state.
            - Try out relative speed scaled melee damage.


- Presentation:
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
        - Render a false projectile exiting the player's barrel.
            - False projectile should converge with the actual projectile over distance.
        - [Critical] Move or disable all FPV weapon lights/effects.
    
    - Sound:
        - [Blocker] Scale Mario sound by game's volume setting.

- Usability:
    - Keybinds:
        - [Blocker] Implement remappable keybinds.
        - [Blocker] Reverse MCC's keybinds so we can hook into those too.
        - [Blocker] Address some keys being detected when window is not focused.

- Tooling
    - Track tags and assets in git.
        - Store diffs relative to vanilla tag values.
        - Export tag data to XML for human readable diffs.
