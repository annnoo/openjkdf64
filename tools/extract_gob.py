import struct
import os
import sys
from pathlib import Path

import argparse

def extract(gob_path, out_dir):
    with open(gob_path, 'rb') as f:
        # Header
        data = f.read(12)
        if len(data) < 12:
            raise ValueError("File too small for GOB header")
            
        magic, version, table_off = struct.unpack('<4sII', data)
        if magic != b'GOB ' and magic != b'GOO ':
            raise ValueError(f"Invalid magic: {magic}")
        
        # Seek to index
        f.seek(table_off)
        num_files_data = f.read(4)
        if not num_files_data:
            raise ValueError("Failed to read number of files at table offset")
        num_files = struct.unpack('<I', num_files_data)[0]
        
        entries = []
        for _ in range(num_files):
            entry_data = f.read(136)
            if len(entry_data) < 136:
                break
            off, size, name_raw = struct.unpack('<II128s', entry_data)
            # Split at first null byte and decode
            name = name_raw.split(b'\0')[0].decode('latin-1').replace('\\', '/')
            entries.append((off, size, name))
            
        # Extract
        CHUNK_SIZE = 64 * 1024
        for off, size, name in entries:
            target = Path(out_dir) / name
            target.parent.mkdir(parents=True, exist_ok=True)
            f.seek(off)
            with open(target, 'wb') as out:
                remaining = size
                while remaining > 0:
                    to_read = min(remaining, CHUNK_SIZE)
                    chunk = f.read(to_read)
                    if not chunk:
                        break
                    out.write(chunk)
                    remaining -= len(chunk)
            print(f"Extracted: {name}")

def main():
    parser = argparse.ArgumentParser(description='Extract Jedi Knight GOB/GOO files')
    parser.add_argument('gob_path', help='Path to the GOB/GOO file')
    parser.add_argument('out_dir', help='Directory to extract files into')
    args = parser.parse_args()
    
    try:
        extract(args.gob_path, args.out_dir)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
