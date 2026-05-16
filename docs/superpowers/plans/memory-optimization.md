# Plan: N64 Memory Optimization and OOM Resolution

## Root Cause Analysis
The Out Of Memory (OOM) failure during Episode 1 Level 1 (Nar Shaddaa) loading is caused by the engine attempting to eagerly load all referenced assets—specifically materials and their associated texture data—into RAM. 

1. **Texture Bloat:** Level 1 references hundreds of textures. Even at 8-bit color depth, these quickly consume several megabytes of RAM, exceeding the base 4MB N64 memory and potentially the 8MB Expansion Pak limit when combined with level geometry (sectors, surfaces).
2. **Metadata Overhead:** Large structures like `rdTexture`, `rdDDrawSurface`, and `rdKeyframe` carry significant overhead (debug strings, unused DirectDraw fields) that adds up when multiplied by hundreds of entries.
3. **Eager Loading:** The current `rdMaterial_Load` implementation allocates `stdVBuffer` objects for every texture and mipmap during level startup.

---

## Proposed Strategies

### 1. Lazy / Deferred Asset Loading (LRU) - RECOMMENDED
Instead of loading pixels or joint data during `sithWorld_Load`, we only load the metadata (headers).
*   **Keyframes:** 
    *   Add `bLoaded` and `fpath` to `rdKeyframe`.
    *   Modify `rdKeyframe_LoadEntry` to skip the massive joint/entry loop during initial load.
    *   Update `rdPuppet_AccumulateJoints` to trigger a demand-load from DFS when an animation starts playing.
*   **Materials:** 
    *   Already partially implemented via `RDMATERIAL_LRU_LOAD_UNLOAD`.
    *   Need to ensure the renderer actually triggers the load.
*   **Pros:** Minimal startup RAM, allows huge levels.
*   **Cons:** Performance spikes/stuttering when new assets are loaded during gameplay.

### 2. Memory Model Optimization (Stripping)
Shrink the footprint of all engine structures and ensure maximum RAM availability.
*   **Implementation:**
    *   Define `OPTIMIZE_AWAY_UNUSED_FIELDS` to strip legacy Win32/DirectDraw fields (DONE).
    *   Disable `SITH_DEBUG_STRUCT_NAMES` to remove 32-byte name strings (DONE).
    *   **Strip `rdAnimEntry`:** Remove `vel` and `angVel` from animation entries on N64. This saves 24 bytes per frame per joint (~43% reduction in animation RAM).
*   **Pros:** Low implementation risk, maintains original engine behavior.
*   **Cons:** Loss of some motion smoothness (if velocity interpolation was significant).

### 3. Keyframe Downsampling / Quantization
Reduce the precision or frequency of the loaded data.
*   **Implementation:**
    *   Modify `rdKeyframe_LoadEntry` to only load every Nth frame, or convert `rdVector3` (12 bytes) to `int16` based quantization (6 bytes).
*   **Pros:** Predictable RAM savings.
*   **Cons:** Visually jittery animations.

---

## Recommendations

### Short-Term (The "Easy Way" to get running)
1.  **Keyframe Lazy Loading:** This is the most effective way to bypass the OOM. Since Level 1 has 201 keyframes but only a few are active at any time, this will save several megabytes of RAM immediately.
2.  **Strip `rdAnimEntry`:** Remove the `vel` and `angVel` fields. Most puppet animations don't strictly require these for visual correctness on a small screen.

### Long-Term (For stable gameplay)
1.  **Full LRU Cache:** Implement a manager that unloads oldest assets when RAM is low.
2.  **DFS-Direct Uploads:** Optimize the renderer to use PI DMA to stream texture data directly to the RDP.

---

## Recommendations

### Short-Term (To get Level 1 running)
1.  **Metadata Stripping:** Enable `OPTIMIZE_AWAY_UNUSED_FIELDS` and disable debug strings. This is a surgical change that can save ~100-200KB immediately.
2.  **Partial Texture Loading:** Implement the `TARGET_TWL` logic where we only load small textures or skip large mipmaps during initial load. This is the fastest way to bypass the OOM and see the 3D world.
3.  **Expansion Pak Check:** Verify and enforce 8MB mode.

### Long-Term (For stable gameplay)
1.  **Full LRU Cache:** Implement `RDMATERIAL_LRU_LOAD_UNLOAD` properly so that textures are only in RAM when they are actually needed in the current frustum or scene.
2.  **DFS-Direct Uploads:** Optimize the renderer to use PI DMA to stream texture data directly to the RDP, reducing RAM requirements for static assets to nearly zero.

---

## Immediate Next Steps
1.  Enable structure stripping in `engine_config.h`.
2.  Port the TWL "small texture" limit logic to `rdMaterial.c`.
3.  Add a Heap Reporter to the N64 platform layer to monitor RAM usage in real-time.
