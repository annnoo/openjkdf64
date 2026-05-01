# Audio Subsystem Architecture

The Audio system relies on triggering sound effects attached to entities and playing streaming background tracks.

## Core Files (`src/Devices/sithSound.c`, `stdSound.c`)
- **Sound Classes (`sithSoundClass.c`):** Sound effects are defined in `soundclass` files (`.snd`). These map logical sound IDs (e.g., `fire_weapon`, `footstep`) to `.wav` files.
- **Positional Audio:** Sounds emitted by a `sithThing` have a 3D coordinate. The engine calculates volume attenuation based on the distance between the camera and the emitting entity.
- **Music:** Streams `.wav` or `.ogg` files dynamically, looping based on game state (Combat vs. Ambient).

## N64 Porting Strategy (Libdragon)
- **Replace OpenAL/SDL_mixer:** Strip all references to PC audio libraries.
- **Libdragon Mixer:** Implement `libdragon`'s `mixer` and `wav64` APIs.
- **Asset Pipeline:** PC `.wav` files are too large for N64 ROMs. Team 1 must compress all audio assets into `wav64` format (ADPCM or IMA ADPCM compression) using `libdragon`'s `audioconv64` tool.
- **3D Spatialization:** `libdragon` does not natively spatialize audio like OpenAL. Team 2 must manually calculate left/right channel panning and volume attenuation based on the listener's `rdMatrix34` (camera orientation) and the sound emitter's `rdVector3`.
- **Memory Constraint:** Never load entire tracks into RAM. Small sound effects (blasters, jumps) can be cached in a 1MB audio arena, but CD-quality background music must be streamed in chunks via DMA from the ROM.