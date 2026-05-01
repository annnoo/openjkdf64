# AI System Architecture

The OpenJKDF2 AI subsystem handles non-player character (NPC) behavior, movement, targeting, and state machines.

## Core Logic Flow

### 1. The AI Tick (`sithAI_Tick`)
Called every engine tick for each active enemy. 
- Evaluates the enemy's current state (e.g., Idle, Patrol, Attack, Flee).
- Updates timers and awareness (sight, hearing).

### 2. Awareness and Senses (`sithAIAwareness.c`)
- **Sight:** Uses the world's raycasting logic to determine if a line-of-sight exists between the enemy's eye coordinates and the player's bounding box.
- **Hearing:** Triggered by weapon fire or footsteps, propagating sound events across sector adjoins.

### 3. Pathfinding & Movement
- **Adjoin Traversal:** Enemies navigate by moving towards sector adjoins (portals). `sithAI` calculates waypoints based on the center points of adjoin surfaces.
- **Collision Avoidance:** Uses bounding box intersection (`sithSector_ThingCollision`) to prevent clipping into walls.

### 4. Behaviors (`sithAIClass.c`)
Defined in AI class files (`.ai`).
- Maps engine events (e.g., taking damage) to state transitions.
- Dictates accuracy, fire rate, and movement speed.

## N64 Porting Strategy
- **Compute Budget:** Pathfinding and line-of-sight raycasts are mathematically heavy. The N64 engine tick must heavily throttle AI updates. Instead of raycasting every frame for every enemy, raycasts should be staggered (e.g., check 2 enemies per frame, queueing the rest).
- **Floating Point:** The VR4300 has a hardware FPU. Pathfinding vectors (`rdVector3`) can remain as `float` variables, but avoid excessive `sqrtf` operations in distance checks (use squared distance instead).