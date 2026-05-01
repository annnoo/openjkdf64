# World and Game Logic Architecture

The `sith` layer handles everything that makes Jedi Knight a game.

## World Entities (Things)

Everything in the game world is a `sithThing`.
- **Types**: Actors (Players/Enemies), Weapons, Items, Explosions, Particles, and static Decorations.
- **Properties**:
    - **Position/Orientation**: `lookOrientation`, `position`.
    - **Physics**: `sithThingPhysParams` handles velocity, gravity, and collision response.
    - **Logic**: `controlType` determines if the thing is controlled by the player, AI, or scripts.

## Scripting (COG)

Jedi Knight uses a custom scripting language called **COG**.
- **Source**: `src/Cog`
- **Compiler**: Uses `byacc` and `flex` to parse scripts.
- **Runtime**: `sithCogExec.c` executes the bytecode.
- **Functions**: `sithCogFunction.c` provides the API that scripts use to interact with the game (e.g., `SendMessage`, `GetThingPos`, `SetWallCel`).
- **Events**: Scripts are event-driven (e.g., `activated:`, `touched:`, `timer:`).

## AI and Actors

- **Instincts**: Actors have "instincts" that determine their behavior (e.g., searching for the player, fleeing, attacking).
- **Paths**: Actors follow pathing data defined in the level (JKL).

## Physics and Collision

- **Collision**: `sithCollision.c` handles swept-sphere vs. surface and sphere-vs-sphere tests.
- **Physics**: `sithPhysics.c` applies forces, gravity, and friction. It is tightly integrated with the sector/surface geometry to handle sliding and stepping.
