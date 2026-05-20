- Fall damage:
    - Disable fall death timer for Cheif,
    - Disable fall damage for Cheif.
    - Apply Mario's fall damage to Cheif.

- Disable or move 1rst person weapon flares and muzzle flashes.

- Implement collision with vehicles, bipeds and other moving objects.

- Implement melee and shell damage interactions.

- Implement damage knockback on Mario.

- Shrink Chief to Mario's size so AI targetting looks correct.

- Implement level geometry chunking. Keep Mario's coordinates near the origin to avoid overflow.

- Render a false projectile exiting the player's barrel.
    - False projectile should converge with the actual projectile over distance.
    
- Fix wall kicks for near-vertical walls.
    - Place a vertical wall between Mario and a wall when he is airborne and close.

- Blend Mario's animation with Chief's.

- Implement slow motion.

Done:

x Keep mario model's lighting up to date.
x Fix Mario's shadow clipping.
x Fix spurious collisions with large distant triangles.