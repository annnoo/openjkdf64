# N64 Keyframe Lazy Loading Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the existing N64 Keyframe Lazy Loading implementation to correctly preserve metadata (like `id`) during demand-loading and ensure it successfully bypasses OOM during level startup.

**Architecture:** 
1. Modify `rdKeyframe_LoadEntry` to preserve critical metadata fields (`id`, `fpath`) when re-loading a keyframe that was previously metadata-only.
2. Verify that `rdKeyframe_EnsureLoaded` correctly triggers when an animation is played.
3. (Optional Bonus) Implement similar lazy loading for `rdModel3` if RAM is still tight.

**Tech Stack:** C, N64 Libdragon.

---

### Task 1: Fix `rdKeyframe_LoadEntry` Metadata Preservation

**Files:**
- Modify: `src/Engine/rdKeyframe.c`

- [ ] **Step 1: Update `rdKeyframe_LoadEntry` to preserve `id` and `fpath`**

```c
// Inside rdKeyframe_LoadEntry:
    int prevId = keyframe->id;
#ifdef TARGET_N64
    char prevFpath[64];
    stdString_SafeStrCopy(prevFpath, keyframe->fpath, sizeof(prevFpath));
#endif

    rdKeyframe_NewEntry(keyframe);

    // Restore preserved fields
    keyframe->id = prevId;
#ifdef TARGET_N64
    stdString_SafeStrCopy(keyframe->fpath, prevFpath, sizeof(keyframe->fpath));
#endif
```

- [ ] **Step 2: Ensure `bLoaded` is correctly set**
The existing code already has:
```c
#ifdef TARGET_N64
    if (sithWorld_pLoading) {
        keyframe->bLoaded = 0;
        ...
        return 1;
    }
    keyframe->bLoaded = 1;
#endif
```
Verify this logic is robust. If `sithWorld_pLoading` is temporarily NULL in `rdKeyframe_EnsureLoaded`, it correctly proceeds to full load.

- [ ] **Step 3: Verify and Commit**
```bash
git add src/Engine/rdKeyframe.c
git commit -m "fix(n64): preserve keyframe id and fpath during lazy loading"
```

### Task 2: Implement Lazy Loading for Models (rdModel3)

**Files:**
- Modify: `src/types.h`
- Modify: `src/Primitives/rdModel3.c`

- [ ] **Step 1: Add `bLoaded` and `fpath` to `rdModel3` struct**
```c
// src/types.h around line 390
typedef struct rdModel3
{
    ...
#ifdef TARGET_N64
    char fpath[64];
    int bLoaded;
#endif
} rdModel3;
```

- [ ] **Step 2: Update `rdModel3_Load` to support short-circuiting**
Similar to `rdKeyframe_LoadEntry`, if `sithWorld_pLoading` is non-NULL, read only the header/radius and return early.

- [ ] **Step 3: Implement `rdModel3_EnsureLoaded`**
```c
int rdModel3_EnsureLoaded(rdModel3 *model) {
    if (!model || model->bLoaded) return 1;
    // ... same logic as rdKeyframe_EnsureLoaded ...
}
```

- [ ] **Step 4: Update renderer to call `rdModel3_EnsureLoaded`**
In `rdRaster_DrawModel` or equivalent, ensure model is loaded before drawing.

- [ ] **Step 5: Commit**
```bash
git add src/types.h src/Primitives/rdModel3.c
git commit -m "feat(n64): implement lazy loading for 3DO models"
```

### Task 3: Final Verification and OOM Check

- [ ] **Step 1: Rebuild and test on N64**
Check if `01narshadda.jkl` now passes the `things` section.
Expected: Heap free space > 300 KB before `things` load.
