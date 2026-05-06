import os
import sys
from pathlib import Path

def generate_conversion_script(input_dir, script_path, output_dir):
    """
    Scans for .wav files and generates a bash script to convert them using audioconv64.
    """
    input_path = Path(input_dir)
    wav_files = list(input_path.rglob("*.wav"))
    
    if not wav_files:
        print(f"No .wav files found in {input_dir}")
        return

    print(f"Found {len(wav_files)} audio files. Generating {script_path}...")
    
    with open(script_path, "w") as f:
        f.write("#!/bin/bash\n")
        f.write(f"# Automated audio conversion for OpenJKDF2-N64\n\n")
        f.write(f"mkdir -p {output_dir}\n\n")
        
        for wav in wav_files:
            # Create a relative path for the output to preserve structure if desired,
            # but for now we'll mirror the folder structure in the output.
            rel_path = wav.relative_to(input_path)
            target_path = Path(output_dir) / rel_path.with_suffix(".wav64")
            
            # Ensure target subdirectory exists
            f.write(f"mkdir -p \"{target_path.parent}\"\n")
            
            # audioconv64 command
            # Note: audioconv64 usually outputs to a specific format; 
            # we assume standard usage here.
            f.write(f"audioconv64 -o \"{target_path}\" \"{wav}\"\n")

    # Make the script executable
    os.chmod(script_path, 0o755)
    print("Done.")

if __name__ == "__main__":
    src = "build/assets_raw"
    script = "convert_audio.sh"
    dst = "build/assets"
    
    if len(sys.argv) > 1:
        src = sys.argv[1]
    
    generate_conversion_script(src, script, dst)
