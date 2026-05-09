import os
import re
import shutil
import sys
from pathlib import Path

def get_refs(file_path):
    if not os.path.exists(file_path):
        return set()
    with open(file_path, 'r', errors='ignore') as f:
        content = f.read()
    # Match .3do and .mat filenames
    refs = re.findall(r'[\w-]+\.(?:3do|mat|snd|wav)', content, re.IGNORECASE)
    return {r.lower() for r in refs}

def filter_assets(jkl_path, src_dir, dst_dir):
    print(f"Filtering assets for {jkl_path}...")
    referenced = get_refs(jkl_path)
    
    # Also parse 3dos for mats
    all_3dos = {r for r in referenced if r.endswith('.3do')}
    parsed_3dos = set()
    
    while all_3dos:
        current = all_3dos.pop()
        if current in parsed_3dos:
            continue
        parsed_3dos.add(current)
        path = Path(src_dir) / '3do' / current
        if path.exists():
            new_refs = get_refs(path)
            referenced.update(new_refs)
            all_3dos.update({r for r in new_refs if r.endswith('.3do')})

    # Sync
    for ref in referenced:
        subdir = ''
        if ref.endswith('.3do'): subdir = '3do'
        elif ref.endswith('.mat'): subdir = 'mat'
        # elif ref.endswith('.snd'): subdir = 'sound'
        # elif ref.endswith('.wav'): subdir = 'sound'
        
        if not subdir: continue
        
        src_path = Path(src_dir) / subdir / ref
        if src_path.exists():
            target_path = Path(dst_dir) / subdir / ref
            target_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_path, target_path)
        else:
            # Try voice if sound failed
            if subdir == 'sound':
                src_path = Path(src_dir) / 'voice' / ref
                if src_path.exists():
                    target_path = Path(dst_dir) / 'sound' / ref
                    target_path.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(src_path, target_path)

    print(f"Surgically synced {len(referenced)} assets.")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: filter_level_assets.py <jkl_path> <src_dir> <dst_dir>")
        sys.exit(1)
    filter_assets(sys.argv[1], sys.argv[2], sys.argv[3])
