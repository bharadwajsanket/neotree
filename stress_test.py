#!/usr/bin/env python3
"""
stress_test.py — deep and wide directory stress testing for neotree.
"""

import os
import shutil
import subprocess
import sys

SANDBOX = "stress_sandbox"

def clean_sandbox():
    if os.path.exists(SANDBOX):
        shutil.rmtree(SANDBOX)

def create_deep_dir(depth):
    path = SANDBOX
    for i in range(depth):
        path = os.path.join(path, f"deep_dir_{i}")
    os.makedirs(path, exist_ok=True)
    # create a file at the very bottom
    with open(os.path.join(path, "deep_file.txt"), "w") as f:
        f.write("hello depth")

def create_wide_dir(num_files):
    path = os.path.join(SANDBOX, "wide_dir")
    os.makedirs(path, exist_ok=True)
    for i in range(num_files):
        with open(os.path.join(path, f"file_{i}.txt"), "w") as f:
            f.write(f"content {i}")

def run_neotree(args):
    cmd = ["./neotree"] + args
    print(f"Running: {' '.join(cmd)}")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"neotree failed with code {res.returncode}")
        print(f"stdout:\n{res.stdout}")
        print(f"stderr:\n{res.stderr}")
        sys.exit(1)
    return res.stdout, res.stderr

def main():
    print("============================================")
    print("STARTING STRESS TEST")
    print("============================================")
    
    # 1. Clean and prepare sandbox
    clean_sandbox()
    
    try:
        # 2. Deep recursion stress test (50 levels deep)
        print("Creating 50-level nested directory...")
        create_deep_dir(50)
        
        # Test basic, size, reverse sort, and stats
        run_neotree([SANDBOX])
        run_neotree([SANDBOX, "--size"])
        run_neotree([SANDBOX, "--stats"])
        run_neotree([SANDBOX, "--sort", "size", "--reverse"])
        
        # Test max depth limit
        out, _ = run_neotree([SANDBOX, "-L", "5"])
        # Check that it didn't print deeper directories
        assert "deep_dir_5" not in out, "Max depth limit -L 5 failed to restrict deep walk"
        
        print("Deep directory test passed ✓")
        print("--------------------------------------------")
        
        # 3. Wide directory stress test (5000 files in one directory)
        print("Creating 2000-file wide directory...")
        create_wide_dir(2000)
        
        # Test traversal and sorting of huge lists
        run_neotree([os.path.join(SANDBOX, "wide_dir")])
        run_neotree([os.path.join(SANDBOX, "wide_dir"), "--sort", "size"])
        run_neotree([os.path.join(SANDBOX, "wide_dir"), "--stats"])
        
        print("Wide directory test passed ✓")
        print("============================================")
        print("STRESS TEST SUCCESSFUL!")
        print("============================================")
        
    finally:
        clean_sandbox()

if __name__ == "__main__":
    main()
