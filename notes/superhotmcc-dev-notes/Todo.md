- Code quality:
    - Break down UI.cpp monolith. Audit similar large files.
    - Move all global state into a struct for cleaner reinitilization on halo1.dll reload.
    - Fix file naming inconsistencies (mostly casing).
    - Break up Entity type by type. Eg: Vehicle, Biped, Weapon, etc.
    - Create a fork of LibSM64 that exposes bone transforms.

    - Engine/mod seperation:
        - Move all hooking into the Halo1 namespace. Engine knowledge should not live in mod directories.
        - All mod logic in hooks should be implemented as middleware.

- Bugs and Stability:
    - Implement level geometry chunking. Keep Mario's coordinates near the origin to avoid overflow.
    - Audit all raw memory accesses. Replace with safe wrappers in non-performance critical code.
    - Find out why updateAllEntities is not running in the underground section of Attack on the Control Room.
    x Fix Mario(.weapon) being available for pickup.

- Gameplay:
    - Movement:
        - Implement collision with vehicles, bipeds and other moving objects.
        - Fix wall kicks for near-vertical walls.
            - Place a vertical wall between Mario and a wall when he is airborne and close.
        - Implement ridable jackal shield. (rideable as koopa shell)
        
    - Combat:
        - Implement melee and shell damage interactions.
        - Implement damage knockback on Mario.

    - Implement slow motion.
    - Shrink Chief to Mario's size so AI targetting looks correct.
    - Fall damage:
        - Disable fall death timer for Cheif,
        - Disable fall damage for Cheif.
        - Apply Mario's fall damage to Cheif.

- Presentation:
    - Play sounds from LibSM64.

    - Animation:
        - Inverse kinematics:
            - Arm IK:
                - Mark Mario's arms as not busy when he is reloading/firing (and briefly after), no matter what state he is in.
                - Smooth out arm IK transition from busy to not busy.
                - Extract model markers and use them to pick hand positions for Mario.
                - Implement pull targets so Mario's elbows bend naturally.
            - Torso IK:
                - Try out making Mario lean slightly as he aims up and down.
        - Head:
            - Bias Mario's look direction towards the direction the camera is facing.
            - Lean Mario's head sideways towards weapon when he is firing/ADSing.
        - Look into blend Mario's animation with Chief's.
        - Drive Mario pose from Chief's in cutscenes and vehicles.
        - Look into disabling Halo engine's model interpolation (for non-tick frames).
    
    - Third person camera:
        x Disable or move 1rst person weapon flares and muzzle flashes.
        - Render a false projectile exiting the player's barrel.
            - False projectile should converge with the actual projectile over distance.
            
    - Address Mario jitter during pause.
        - Disable non-tick frame interpolation system.

- Tooling
    - Cheat Engine:
        - Implement external tag browser in Cheat Engine.
        - Implement external entity browser.