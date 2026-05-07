# Refactor GOB Extraction Utility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor the GOB extraction utility for better memory efficiency, robust filename handling, and a professional CLI interface.

**Architecture:**
- Use `argparse` for CLI argument parsing.
- Use `latin-1` for decoding filenames to handle extended characters gracefully.
- Implement a buffered reading loop (64KB chunks) for file extraction.

**Tech Stack:** Python 3, `argparse`, `pathlib`, `struct`.

---

### Task 1: CLI Robustness with `argparse`

**Files:**
- Modify: `tools/extract_gob.py`

- [ ] **Step 1: Replace manual `sys.argv` handling with `argparse`**

```python
import argparse
# ... other imports ...

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
```

- [ ] **Step 2: Verify CLI help message**

Run: `python3 tools/extract_gob.py --help`
Expected: Help message showing arguments.

- [ ] **Step 3: Commit**

```bash
git add tools/extract_gob.py
git commit -m "refactor: use argparse for GOB extraction utility"
```

---

### Task 2: Robust Filename Encoding

**Files:**
- Modify: `tools/extract_gob.py`

- [ ] **Step 1: Update filename decoding from `ascii` to `latin-1`**

```python
# In extract loop:
name = name_raw.split(b'\0')[0].decode('latin-1').replace('\\', '/')
```

- [ ] **Step 2: Commit**

```bash
git add tools/extract_gob.py
git commit -m "feat: use latin-1 for GOB filename decoding"
```

---

### Task 3: Memory Efficient Extraction (Streaming)

**Files:**
- Modify: `tools/extract_gob.py`

- [ ] **Step 1: Refactor extraction loop to use chunks**

```python
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
```

- [ ] **Step 2: Verify with existing tests**

Run: `python3 -m unittest tests/test_extract_gob.py`
Expected: Ran 2 tests. OK

- [ ] **Step 3: Commit**

```bash
git add tools/extract_gob.py
git commit -m "perf: stream GOB extraction in chunks for memory efficiency"
```

---

### Task 4: Final Verification

- [ ] **Step 1: Run all tests one last time**

Run: `python3 -m unittest tests/test_extract_gob.py`
Expected: OK

- [ ] **Step 2: Commit**

```bash
git commit --allow-empty -m "docs: GOB extraction utility refactoring complete"
```
