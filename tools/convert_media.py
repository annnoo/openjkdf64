#!/usr/bin/env python3
"""
convert_media.py — Convert JK:DF2 WAV audio to N64-optimised .wav64 (VADPCM).

Usage:
    python3 tools/convert_media.py [ASSET_DIR] [--dry-run] [--force]

    ASSET_DIR   Root of the extracted GOB tree (default: tools/out)
    --dry-run   Print commands without running them
    --force     Re-convert even if the .wav64 already exists and is newer

audioconv64 must be in PATH (it ships with the libdragon Docker container).

Conversion settings
-------------------
  Compression : VADPCM (level 1) — RSP-decoded, ~2:1 vs 8-bit PCM
  Sample rate : 22050 Hz  (resampled if the source differs)
  Channels    : mono      (JK sources are all mono; stereo would waste space)
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path


# ── Settings ──────────────────────────────────────────────────────────────────

AUDIOCONV = "audioconv64"

# Subdirectories that contain audio (relative to ASSET_DIR)
AUDIO_DIRS = ["sound", "voice"]

# audioconv64 flags applied to every file
CONV_FLAGS = [
    "--wav-compress", "vadpcm",   # VADPCM compression (RSP-decoded on N64)
    "--wav-resample", "22050",    # normalise to 22 050 Hz
    "--wav-mono",                 # force mono (all JK SFX are mono)
]


# ── Helpers ───────────────────────────────────────────────────────────────────

def needs_conversion(src: Path, dst: Path, force: bool) -> bool:
    if force:
        return True
    if not dst.exists():
        return True
    return src.stat().st_mtime > dst.stat().st_mtime


def convert_file(src: Path, dst: Path, dry_run: bool) -> bool:
    """Run audioconv64 on one file. Returns True on success."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    cmd = [AUDIOCONV] + CONV_FLAGS + ["-o", str(dst), str(src)]
    if dry_run:
        print(" ".join(str(c) for c in cmd))
        return True
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR: {src.name}", file=sys.stderr)
        if result.stderr:
            print(result.stderr.rstrip(), file=sys.stderr)
        return False
    return True


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Convert JK:DF2 WAV files to .wav64 (VADPCM) for N64."
    )
    parser.add_argument("asset_dir", nargs="?", default="tools/out",
                        help="Root of extracted GOB tree (default: tools/out)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print commands without executing them")
    parser.add_argument("--force", action="store_true",
                        help="Re-convert even when .wav64 is up-to-date")
    args = parser.parse_args()

    asset_root = Path(args.asset_dir)
    if not asset_root.is_dir():
        sys.exit(f"Error: asset directory not found: {asset_root}")

    # Collect .wav files from all audio subdirectories
    wav_files: list[Path] = []
    for subdir in AUDIO_DIRS:
        wav_files.extend(sorted((asset_root / subdir).rglob("*.wav")))

    if not wav_files:
        print(f"No .wav files found under {asset_root}/({', '.join(AUDIO_DIRS)})")
        print("Run extract_gob.py first to populate the asset tree.")
        sys.exit(0)

    total = len(wav_files)
    skipped = converted = failed = 0

    print(f"Found {total} WAV files in {asset_root}")

    for src in wav_files:
        dst = src.with_suffix(".wav64")
        if not needs_conversion(src, dst, args.force):
            skipped += 1
            continue
        rel = src.relative_to(asset_root)
        print(f"  [{converted + failed + 1}/{total - skipped}] {rel}")
        if convert_file(src, dst, args.dry_run):
            converted += 1
        else:
            failed += 1

    print(f"\nDone: {converted} converted, {skipped} up-to-date, {failed} failed.")
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
