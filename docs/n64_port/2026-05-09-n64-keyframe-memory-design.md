# Spec: N64 Keyframe Memory Optimization

## 1. Context and Problem
During the loading of the `things` section in Level 1 (Nar Shaddaa), the OpenJKDF2 N64 port experiences an Out Of Memory (OOM) failure. 
The N64 has 8MB of RAM (with Expansion Pak), but loading Level 1 consumes over 5.4MB before even reaching the `things` section. This is primarily caused by eager loading of `rdKeyframe` animation data. 

Level 1 references over 200 keyframes.
Each keyframe contains multiple joints, and each joint contains an array of `rdAnimEntry` structures.
A single `rdAnimEntry` is 32 bytes on N64 (since `vel` and `angVel` are already stripped).
For an animation with 15 joints and 30 frames, this equals `15 * 30 * 32 = 14,400 bytes` (14 KB) per keyframe.
Loading 201 keyframes eagerly consumes roughly **2.8 MB of RAM**.

## 2. Evaluation of Approaches

### Approach 1: Lazy Loading (Demand Paging) - *Chosen Approach*
Instead of loading all joint animation entries during level load, we only load the keyframe header metadata. When a puppet actually begins playing an animation, we allocate the required memory and read the animation entries directly from the ROM/DFS. When the level is done, the memory is freed.
- **Estimated Savings:** ~2.7 MB during boot. We only load headers (200 * ~100 bytes = 20 KB). Memory spikes only by ~14 KB per active unique animation during gameplay.
- **N64 Context:** This is standard practice for N64 development. RAM is severely limited, but ROM access via DMA is fast. Games frequently stream animation frames or texture data on demand instead of caching it all.
- **Trade-offs:** Small IO overhead when an animation first plays.

### Approach 2: Data Quantization
Convert the `rdVector3` (12-byte float) properties in `rdAnimEntry` into 6-byte `int16` fixed-point or quantized values.
- **Estimated Savings:** Reduces `rdAnimEntry` size from 32 bytes to 14 bytes (saving ~50%, or 1.4 MB).
- **N64 Context:** Highly common for 3D geometry and animation. The N64 RCP typically operates on 16-bit integer vertices.
- **Trade-offs:** Visually jittery animations due to precision loss. Requires changes to the engine's animation interpolation math.

### Approach 3: Animation Downsampling
Only load every Nth frame (e.g., skip every other frame) and interpolate between them.
- **Estimated Savings:** 50% savings for 2x downsampling (~1.4 MB).
- **N64 Context:** Commonly used to fit games onto smaller cartridges, but less frequently used strictly for RAM savings if streaming is available.
- **Trade-offs:** Jittery or unnatural movement, fast movements might be completely lost.

## 3. Implementation Plan for Approach 1 (Lazy Loading)

We will implement Lazy Loading (Approach 1) as it provides the most significant RAM savings and aligns perfectly with the N64's fast DMA capabilities.

### Changes:
1. **`rdKeyframe` Structure:** Add `bLoaded` and `fpath` to `rdKeyframe` to track state and source file.
2. **`rdKeyframe_LoadEntry`:** Skip reading the `numJoints` * `numFrames` entries. We will just `fseek` past them or return early after populating the header.
3. **`rdPuppet_AccumulateJoints` (or animation playback entry point):** Add a check `if (!keyframe->bLoaded)`. If true, call a new function `rdKeyframe_LoadData(keyframe)` which allocates the joint arrays and reads them from DFS.
4. Ensure the allocated joint data is freed when the keyframe is destroyed in `rdKeyframe_Free`.

## 4. Success Criteria
Level 1 successfully loads the `things` section without crashing or failing `pSithHS->alloc` due to OOM.
