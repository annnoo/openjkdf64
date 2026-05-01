# Resource Management and Data Formats

Jedi Knight uses several proprietary file formats, all of which are handled by the `std` and `rd` layers.

## Archive Files (GOB)
- **Format**: `.gob`
- **Source**: `src/Win95/stdGob.c`
- **Purpose**: A simple uncompressed archive that stores all game assets.
- **Handling**: The engine "mounts" GOB files and treats them like a virtual file system.

## Graphics Assets
- **Textures (MAT)**: `.mat` (Material) files contain one or more textures (cels) and associated properties. They are almost always 8-bit paletted.
- **Bitmaps (BM)**: `.bm` files are used for UI elements and are also usually 8-bit paletted.
- **Palettes (CMP)**: `.cmp` (Colormap) files define the 256-color palettes used by the textures.
- **3D Models (3DO)**: `.3do` files store vertex, face, and node hierarchy data.

## Level Data (JKL)
- **Format**: `.jkl`
- **Purpose**: The main level file containing sector geometry, surface definitions, adjoins, thing placements, and COG script references.

## Audio Assets
- **Sound (WAV)**: The engine uses standard WAV files for sound effects.
- **Music (CD/OGG)**: Original music was on the CD; the port supports Ogg Vorbis.

## Video (SMK/SAN)
- **Format**: Smacker (`.smk`) for cinematics.
- **Source**: `src/Main/jkSmack.c` and `src/Main/smack.h`.
