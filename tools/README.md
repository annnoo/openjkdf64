# OpenJKDF2-N64 Asset Pipeline

This directory contains tools for preparing game assets for the N64 port.
The full pipeline goes from a licensed copy of JK:DF2 to a ready-to-package
`tools/out/` directory that the N64 ROM build consumes.

## Requirements

- Python 3.x
- `audioconv64` (part of the libdragon toolchain, must be in `PATH`)
- CMake + a native C compiler (for `cogc`)

---

## Step-by-step

### 1. Extract game archives

Extract `resource.gob` (and `Res2.gob`, episode `.gob` files, etc.) into the
raw asset tree under `tools/out/`.

```bash
python3 tools/extract_gob.py path/to/resource.gob tools/out
python3 tools/extract_gob.py path/to/Res2.gob     tools/out
# repeat for each .gob / .goo file
```

This populates `tools/out/cog/`, `tools/out/3do/`, `tools/out/mat/`,
`tools/out/sound/`, `tools/out/voice/`, etc.

### 2. Convert audio to N64 format

This is a two-sub-step process handled by `convert_media.py` + the generated
shell script.  See [Audio preprocessing pipeline](#audio-preprocessing-pipeline)
below for the full technical background.

**2a. Generate the conversion script:**

```bash
python3 tools/convert_media.py tools/out
```

`convert_media.py` scans `tools/out` recursively for every `.wav` file and
writes `convert_audio.sh` next to it.  Each line of the generated script calls
`audioconv64` to convert one `.wav` → `.wav64` in-place (same directory tree,
same filename, different extension).

**2b. Run the conversion:**

```bash
./convert_audio.sh
```

After this step every `.wav` under `tools/out/sound/` and `tools/out/voice/`
has a matching `.wav64` alongside it.  The `.wav` originals are **not** deleted
and are **not** packed into the ROM — only `.wav64` files are consumed by the
runtime.

### 3. Compile COG scripts

All `.cog` scripts under `tools/out/cog/` are compiled to binary `.bcog`
files automatically as part of the CMake build.  Configure once, then every
subsequent `cmake --build` re-compiles any changed scripts:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake_modules/toolchain_native.cmake
cmake --build build --target compile_cogs
# or just: cmake --build build   (compile_cogs is part of ALL)
```

Output `.bcog` files are written next to their source `.cog` files:

```
tools/out/cog/
  weap_bryar.cog      <- JK source (not shipped)
  weap_bryar.bcog     <- binary, consumed by the N64 runtime
  ...
```

The N64 runtime loads `.bcog` directly when built with
`-DCOG_USE_PRECOMPILED`.  It hard-fails if a `.bcog` is missing — there is
no fallback to the text parser on device.

### 4. Package for ROM

Once all the above steps are done, `tools/out/` contains everything needed.
Point the N64 ROM's filesystem image builder (DragonFS / mkdfs or similar)
at `tools/out/` to bundle all assets into the ROM:

```bash
mkdfs tools/out.dfs tools/out
```

Then link `tools/out.dfs` into the final `.z64` ROM as usual via the
libdragon CMake rules.

---

## Audio preprocessing pipeline

### Background: why not plain WAV?

JK:DF2 stores all audio as uncompressed PCM WAV files inside `.gob` archives
(typically 8-bit mono, 8 000 – 22 050 Hz).  These are fine on PC with a hard
drive, but are too large and too slow to stream usefully from N64 ROM:

| Audio type | Raw size (typical) | After VADPCM |
|---|---|---|
| Short SFX (< 1 s, 8-bit 11 kHz mono) | ~11 KB | ~3 KB |
| Long SFX / ambient (5 s, 8-bit 22 kHz mono) | ~110 KB | ~28 KB |
| Voice line (3 s, 8-bit 11 kHz mono) | ~33 KB | ~9 KB |

VADPCM gives roughly **4:1 compression vs. 16-bit PCM** or **~2:1 vs. 8-bit
PCM**, and decompression runs entirely on the RSP — the CPU never touches
audio samples during playback.

### The `.wav64` format

`.wav64` is libdragon's container for audio that plays through the RSP mixer.
It is a binary file with a small header followed by compressed or uncompressed
sample data.  The `audioconv64` tool (shipped with libdragon) produces it.

Compression levels supported by `audioconv64 --wav-compress`:

| Level | Name | Description |
|---|---|---|
| `0` | none | Raw PCM (16-bit signed). Largest, no CPU/RSP overhead. |
| `1` | VADPCM (default) | N64-native ADPCM, 4-bit per sample. RSP-decoded. Recommended for all SFX. |
| `3` | Opus | Perceptual codec for long music/voice. Higher quality-per-byte but requires extra RSP init (`wav64_init_compression(3)`). |

For OpenJKDF2-N64 all SFX and voice lines use **level 1 (VADPCM)**.

Optional VADPCM sub-flags (append after the level with a comma):

```
--wav-compress vadpcm,huffman=true   # Huffman-code the VADPCM residuals (~5-10% extra)
--wav-compress vadpcm,bits=3         # 3 bits/sample instead of 4 (more compression, lower quality)
```

### What `convert_media.py` does

1. Recursively walks the `tools/out` directory for `*.wav` files.
2. For each file, emits two lines in `convert_audio.sh`:
   - `mkdir -p <output_dir>` — ensures the mirrored subdirectory exists.
   - `audioconv64 -o <path>.wav64 <path>.wav` — runs the converter.
3. Marks the script executable (`chmod 755`).

The output `.wav64` file is written **next to** the source `.wav` with the
same stem, preserving the full subdirectory structure so the runtime can find
files by the same relative path.

Example (abbreviated `convert_audio.sh`):
```bash
#!/bin/bash
mkdir -p tools/out/sound/
audioconv64 -o "tools/out/sound/activate.wav64"  "tools/out/sound/activate.wav"
audioconv64 -o "tools/out/sound/amb_wind.wav64"  "tools/out/sound/amb_wind.wav"
mkdir -p tools/out/voice/
audioconv64 -o "tools/out/voice/01kyell0.wav64"  "tools/out/voice/01kyell0.wav"
...
```

### Runtime playback (planned `stdSound_N64.c`)

The N64 runtime side uses the **libdragon RSP mixer** (`mixer.h` / `wav64.h`):

```
audio_init(22050, 4)    ← init AI hardware: 22 050 Hz output, 4 back-buffers
mixer_init(32)          ← allocate 32 mix channels (channels 0-1 reserved for music)
```

Per frame (in `Window_Main_Loop`):
```
mixer_try_play()        ← pump the mixer; must be called frequently to avoid stutter
```

Per sound effect:
```
wav64_open(&snd, "rom:/sound/activate.wav64")   ← open from DFS (streams from ROM)
wav64_play(&snd, ch)                            ← assign to a free channel and start
mixer_ch_set_vol_pan(ch, vol, pan)              ← volume / stereo pan
mixer_ch_set_freq(ch, hz)                       ← pitch-shift (Doppler, etc.)
mixer_ch_stop(ch)                               ← stop early if needed
```

`wav64_t` objects are kept alive for as long as the sound may play.
`wav64_close` is called when the sound is unloaded (e.g., level unload).

Channel allocation strategy:
- Channel **0–1** — stereo music / ambient loops.
- Channels **2–31** — pooled SFX.  A bitmask tracks which are free; the engine
  picks the lowest free channel and records the mapping `stdSound_buffer_t →
  channel index`.

### Looping sounds

Looping ambient sounds (wind, water, hum) pass `--wav-loop true` to
`audioconv64` at conversion time, **or** call `wav64_set_loop(&snd, true)` at
runtime before playing.  The loop restarts from sample 0 unless
`--wav-loop-offset N` was used to set a non-zero loop point.

### Seeking / music

Long music tracks can use `--wav-seek FILE` to embed a seek table, enabling
`wav64_seek(&snd, ch, time_sec)` to jump to any point without decoding from
the beginning.  This is not needed for SFX.

---

## Directory layout after the full pipeline

```
tools/out/
  cog/        COG scripts (.cog) + compiled bytecode (.bcog)
  3do/        3D object models
  jkl/        Level files
  mat/        Material/texture files
  misc/       Miscellaneous data files
  sound/      SFX: .wav (source, ignored by ROM) + .wav64 (packed into ROM)
  ui/         UI graphics
  voice/      Voice: .wav (source, ignored by ROM) + .wav64 (packed into ROM)
```

---

## Archive format (GOB/GOO)

`extract_gob.py` parses the LucasArts GOB/GOO format:

- **Magic:** `GOB ` or `GOO `
- **Version:** 20 (0x14)
- **Indexing:** Offset-based file entry table with 128-byte filenames.
- **Legacy handling:** Strips the `0x1A` EOF byte from extracted files.
