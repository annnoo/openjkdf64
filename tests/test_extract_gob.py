import struct
import io
import unittest
import os
import shutil
from pathlib import Path
from tools.extract_gob import extract

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

    def test_extract(self):
        test_dir = Path("test_out")
        if test_dir.exists():
            shutil.rmtree(test_dir)
        test_dir.mkdir()
        
        gob_file = "test.gob"
        
        # Create a fake GOB
        # Header: Magic, Version 20, Table Offset 32
        header = struct.pack('<4sII', b'GOB ', 20, 32)
        
        # File 1 data: "Hello" at offset 12, size 5
        file1_data = b"Hello"
        # File 2 data: "World" at offset 17, size 5
        file2_data = b"World"
        
        # Table: num_files=2, then entries
        num_files = 2
        table_header = struct.pack('<I', num_files)
        
        # Entry 1: offset 12, size 5, name "test1.txt"
        name1 = b"test1.txt" + b"\0" * (128 - len("test1.txt"))
        entry1 = struct.pack('<II128s', 12, 5, name1)
        
        # Entry 2: offset 17, size 5, name "subdir\\test2.txt"
        name2 = b"subdir\\test2.txt" + b"\0" * (128 - len("subdir\\test2.txt"))
        entry2 = struct.pack('<II128s', 17, 5, name2)
        
        with open(gob_file, 'wb') as f:
            f.write(header)        # 0-11
            f.write(file1_data)    # 12-16
            f.write(file2_data)    # 17-21
            f.seek(32)
            f.write(table_header)  # 32-35
            f.write(entry1)        # 36-171
            f.write(entry2)        # 172-307
            
        try:
            extract(gob_file, str(test_dir))
            
            with open(test_dir / "test1.txt", 'rb') as f:
                self.assertEqual(f.read(), b"Hello")
                
            with open(test_dir / "subdir" / "test2.txt", 'rb') as f:
                self.assertEqual(f.read(), b"World")
                
        finally:
            if os.path.exists(gob_file):
                os.remove(gob_file)
            if test_dir.exists():
                shutil.rmtree(test_dir)

    def test_latin1_filename(self):
        test_dir = Path("test_out_latin1")
        if test_dir.exists():
            shutil.rmtree(test_dir)
        test_dir.mkdir()
        
        gob_file = "test_latin1.gob"
        
        # Create a fake GOB with latin-1 filename
        header = struct.pack('<4sII', b'GOB ', 20, 32)
        file_data = b"Latin1 content"
        num_files = 1
        table_header = struct.pack('<I', num_files)
        
        # "tést.txt" in latin-1 is b't\xe9st.txt'
        name_bytes = b"t\xe9st.txt"
        name = name_bytes + b"\0" * (128 - len(name_bytes))
        entry = struct.pack('<II128s', 12, len(file_data), name)
        
        with open(gob_file, 'wb') as f:
            f.write(header)
            f.write(file_data)
            f.seek(32)
            f.write(table_header)
            f.write(entry)
            
        try:
            extract(gob_file, str(test_dir))
            
            # The extracted file should have the name "tést.txt"
            extracted_path = test_dir / "tést.txt"
            self.assertTrue(extracted_path.exists())
            with open(extracted_path, 'rb') as f:
                self.assertEqual(f.read(), file_data)
                
        finally:
            if os.path.exists(gob_file):
                os.remove(gob_file)
            if test_dir.exists():
                shutil.rmtree(test_dir)

if __name__ == '__main__':
    unittest.main()
