#!/usr/bin/env python3
"""
fuzz_test.py — fuzzing neotree with random, emoji, unicode, and space-filled filenames.
"""

import json
import os
import shutil
import subprocess
import sys

SANDBOX = "fuzz_sandbox"

# List of edge-case filenames to create
EDGE_FILENAMES = [
    "regular_file.txt",
    "file with spaces.txt",
    "   leading_and_trailing_spaces   .txt",
    "emoji_🔥_test.txt",
    "unicode_日本語_test.txt",
    "unicode_русский_test.txt",
    "quote_'_test.txt",
    "double_\"_test.txt",
    "bracket_[_test.txt",
    "backslash_\\_test.txt",
    "a" * 250 + ".txt",  # very long filename close to max limits
]

def clean_sandbox():
    if os.path.exists(SANDBOX):
        shutil.rmtree(SANDBOX)

def create_fuzz_dir():
    os.makedirs(SANDBOX, exist_ok=True)
    # Create files
    for name in EDGE_FILENAMES:
        try:
            with open(os.path.join(SANDBOX, name), "w") as f:
                f.write("fuzz content")
        except OSError as e:
            # Some file systems don't support specific character combinations (e.g. backslashes on Windows)
            # We skip gracefully
            print(f"Skipping file creation for '{name}' due to filesystem limitations: {e}")

    # Create empty directory
    os.makedirs(os.path.join(SANDBOX, "empty_dir"), exist_ok=True)

    # Create hidden files
    with open(os.path.join(SANDBOX, ".hidden_file"), "w") as f:
        f.write("hidden")
    os.makedirs(os.path.join(SANDBOX, ".hidden_dir"), exist_ok=True)

def run_neotree(args):
    cmd = ["./neotree"] + args
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"neotree failed with code {res.returncode}")
        print(f"stdout:\n{res.stdout}")
        print(f"stderr:\n{res.stderr}")
        sys.exit(1)
    return res.stdout, res.stderr

def main():
    print("============================================")
    print("STARTING FUZZ TEST")
    print("============================================")
    
    clean_sandbox()
    create_fuzz_dir()

    try:
        # Run visual rendering with various flags
        print("Testing basic rendering on fuzzed sandbox...")
        run_neotree([SANDBOX])
        
        print("Testing --all on fuzzed sandbox...")
        run_neotree([SANDBOX, "--all"])
        
        print("Testing --size on fuzzed sandbox...")
        run_neotree([SANDBOX, "--size"])
        
        print("Testing --stats on fuzzed sandbox...")
        run_neotree([SANDBOX, "--stats"])
        
        # Test JSON validation
        print("Testing JSON export and validation...")
        json_file = "fuzz_export.json"
        if os.path.exists(json_file):
            os.remove(json_file)
            
        run_neotree([SANDBOX, "--all", "--export-json", json_file])
        
        # Verify JSON is valid
        assert os.path.exists(json_file), "JSON export file was not created"
        with open(json_file, "r") as f:
            data = json.load(f)
            
        # Basic JSON structure validation
        assert "name" in data, "Root has no name"
        assert "children" in data, "Root has no children"
        assert isinstance(data["children"], list), "children is not a list"
        
        print("JSON validation passed ✓")
        
        if os.path.exists(json_file):
            os.remove(json_file)
            
        print("Fuzz test completed successfully!")
        print("============================================")
        print("FUZZ TEST SUCCESSFUL!")
        print("============================================")
        
    finally:
        clean_sandbox()

if __name__ == "__main__":
    main()
