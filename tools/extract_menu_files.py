#!/usr/bin/env python3
"""
extract_menu_files.py — Extract minimum files needed for the JK main menu
from the game's GOB archives.

Usage:
    python3 tools/extract_menu_files.py [--gobdir PATH] [--outdir PATH]

GOB format:
    Offset 0:  4 bytes  magic = "GOB "
    Offset 4:  4 bytes  version = 0x14
    Offset 8:  4 bytes  entry_table_offset (little-endian, always 0x0c)
    At entry_table_offset:
      4 bytes  num_files (LE)
      num_files × stdGobEntry:
        uint32_t fileOffset   (LE)
        int32_t  fileSize     (LE)
        char     fname[128]   (null-terminated, Windows-style backslash paths)
"""

import os
import struct
import argparse
import sys

# ---------------------------------------------------------------------------
# File matching: extract ALL files under ui\bm\, ui\sft\, and specific ui\ files
# ---------------------------------------------------------------------------
WANTED_EXACT = {
    "ui\\jkstrings.uni",
    "ui\\cutstrings.uni",
    "ui\\credits.uni",
}

def is_wanted(fname):
    """Return True if this GOB entry should be extracted."""
    key = fname.lower().replace("/", "\\")
    if key in {w.lower() for w in WANTED_EXACT}:
        return True
    if key.startswith("ui\\bm\\") and key.endswith(".bm"):
        return True
    if key.startswith("ui\\sft\\") and key.endswith(".sft"):
        return True
    if key == "jkl\\static.jkl":
        return True
    if key == "misc\\items.dat":
        return True
    return False


def parse_gob(path):
    """Parse a GOB file and yield (original_fname, offset, size) tuples."""
    with open(path, "rb") as f:
        magic = f.read(4)
        if magic != b"GOB ":
            raise ValueError(f"{path}: not a GOB file (magic={magic!r})")
        version = struct.unpack("<I", f.read(4))[0]
        if version != 0x14:
            print(f"  WARNING: unexpected GOB version 0x{version:02x} (expected 0x14)")
        entry_table_offset = struct.unpack("<I", f.read(4))[0]

        f.seek(entry_table_offset)
        num_files = struct.unpack("<I", f.read(4))[0]
        print(f"  {num_files} entries in {os.path.basename(path)}")

        for _ in range(num_files):
            file_offset = struct.unpack("<I", f.read(4))[0]
            file_size   = struct.unpack("<i", f.read(4))[0]
            fname_raw   = f.read(128)
            fname = fname_raw.split(b"\x00")[0].decode("latin-1")
            yield fname, file_offset, file_size


def extract_from_gob(gob_path, outdir, seen):
    """Extract wanted files from a single GOB into outdir.
    seen: set of lowercase keys already extracted (skip duplicates).
    Returns the set of keys newly extracted from this GOB.
    """
    found = set()
    with open(gob_path, "rb") as f:
        for fname, offset, size in parse_gob(gob_path):
            key = fname.lower().replace("\\", "/")
            if not is_wanted(fname):
                continue
            if key in seen:
                continue

            # Convert Windows path separators and build output path
            rel_path = fname.replace("\\", os.sep).lower()
            out_path = os.path.join(outdir, rel_path)
            os.makedirs(os.path.dirname(out_path), exist_ok=True)

            f.seek(offset)
            data = f.read(size)
            with open(out_path, "wb") as out:
                out.write(data)

            print(f"  extracted: {rel_path} ({size} bytes)")
            found.add(key)

    return found


def main():
    parser = argparse.ArgumentParser(description="Extract JK menu files from GOB archives")
    parser.add_argument("--gobdir", default="/home/anno/Code/gamedev/openjkdf2-gamefiles/jk/Resource",
                        help="Directory containing Res*.gob files")
    parser.add_argument("--outdir", default="filesystem",
                        help="Output directory for extracted files")
    args = parser.parse_args()

    gobdir = args.gobdir
    outdir = args.outdir

    # GOBs searched in priority order — Res2.gob has most UI assets
    gob_order = ["Res2.gob", "Res1hi.gob", "Res3.gob", "Res4.gob"]

    remaining = set()
    total_found = set()

    for gob_name in gob_order:
        gob_path = os.path.join(gobdir, gob_name)
        if not os.path.exists(gob_path):
            print(f"[SKIP] {gob_path} not found")
            continue

        print(f"\n[GOB] {gob_path}")
        try:
            found = extract_from_gob(gob_path, outdir, total_found)
        except ValueError as e:
            print(f"  ERROR: {e}")
            continue

        total_found |= found

    print(f"\n{'='*60}")
    print(f"Extracted {len(total_found)} files to '{outdir}/'")


if __name__ == "__main__":
    main()
