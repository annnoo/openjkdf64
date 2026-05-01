# Data Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a Python-based extraction and media conversion pipeline for OpenJKDF2 N64 assets.

**Architecture:** Use `struct` for binary parsing of GOB/GOO archives and `os.walk` for media scanning. The media tool will generate a Makefile-compatible rules file for `audioconv64`.

**Tech Stack:** Python 3, `struct` module, `pathlib`.

---

### Task 1: GOB Extraction Utility

**Files:**
- Create: `tools/extract_gob.py`
- Test: `tests/test_extract_gob.py`

- [ ] **Step 1: Write a basic test to verify GOB parsing logic**

```python
import struct
import io
import unittest

def parse_gob_header(data):
    magic, version, table_off = struct.unpack('<4sII', data[:12])
    return magic.decode('ascii'), version, table_off

class TestGob(unittest.TestCase):
    def test_header(self):
        # Magic "GOB ", Version 20, Table Offset 20
        data = b'GOB \x14\x00\x00\x00\x14\x00\x00\x00'
        magic, ver, off = parse_gob_header(data)
        self.assertEqual(magic, "GOB ")
        self.assertEqual(ver, 20)
        self.assertEqual(off, 20)

if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run test to verify it passes**

Run: `python3 tests/test_extract_gob.py`

- [ ] **Step 3: Implement the full `extract_gob.py` script**

```python
import struct
import os
import sys
from pathlib import Path

def extract(gob_path, out_dir):
    with open(gob_path, 'rb') as f:
        # Header
        magic, version, table_off = struct.unpack('<4sII', f.read(12))
        if magic != b'GOB ' and magic != b'GOO ':
            raise ValueError(f"Invalid magic: {magic}")
        
        # Seek to index
        f.seek(table_off)
        num_files = struct.unpack('<I', f.read(4))[0]
        
        entries = []
        for _ in range(num_files):
            off, size, name_raw = struct.unpack('<II128s', f.read(136))
            name = name_raw.split(b'\0')[0].decode('ascii').replace('\\', '/')
            entries.append((off, size, name))
            
        # Extract
        for off, size, name in entries:
            target = Path(out_dir) / name
            target.parent.mkdir(parents=True, exist_ok=True)
            f.seek(off)
            with open(target, 'wb') as out:
                out.write(f.read(size))
            print(f"Extracted: {name}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: extract_gob.py <file.gob> <out_dir>")
        sys.exit(1)
    extract(sys.argv[1], sys.argv[2])
```

- [ ] **Step 4: Commit**

```bash
git add tools/extract_gob.py
git commit -m "feat(tools): add GOB/GOO extraction script"
```

---

### Task 2: Media Build Rule Generator

**Files:**
- Create: `tools/convert_media.py`

- [ ] **Step 1: Implement rule generation logic**

```python
import os
import sys
from pathlib import Path

def generate_rules(assets_dir, output_file):
    rules = []
    assets_path = Path(assets_dir)
    
    for root, _, files in os.walk(assets_dir):
        for f in files:
            if f.lower().endswith('.wav'):
                rel_path = Path(root).relative_to(assets_path) / f
                target = Path("build/assets_n64") / rel_path.with_suffix('.wav64')
                source = Path(assets_dir) / rel_path
                
                rule = f"{target}: {source}\n"
                rule += f"\t@mkdir -p $(dir $@)\n"
                rule += f"\taudioconv64 -o $(dir $@) $<\n"
                rules.append(rule)
                
    with open(output_file, 'w') as f:
        f.write("# Generated media rules\n")
        f.write("MEDIA_TARGETS = " + " ".join([r.split(':')[0] for r in rules]) + "\n\n")
        f.writelines("\n".join(rules))

if __name__ == "__main__":
    generate_rules("build/assets_raw", "media_rules.mk")
```

- [ ] **Step 2: Commit**

```bash
git add tools/convert_media.py
git commit -m "feat(tools): add media conversion rule generator"
```

---

### Task 3: Pipeline Documentation

**Files:**
- Create: `tools/README.md`

- [ ] **Step 1: Write README**

```markdown
# OpenJKDF2 N64 Data Pipeline

## 1. Extraction
Extract the game archives:
```bash
python3 tools/extract_gob.py /path/to/resource.gob build/assets_raw
```

## 2. Generate Build Rules
Scan for media and generate Makefile rules:
```bash
python3 tools/convert_media.py
```

## 3. Build
Include the generated `media_rules.mk` in your N64 Makefile:
```make
include media_rules.mk
assets: $(MEDIA_TARGETS)
```
```

- [ ] **Step 2: Commit**

```bash
git add tools/README.md
git commit -m "docs(tools): add pipeline documentation"
```
