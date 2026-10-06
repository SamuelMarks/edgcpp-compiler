#!/usr/bin/env python3
import subprocess
import os
import sys

def test_driver_flag():
    # Only run full build checks if explicitly requested or in a CI environment
    if os.environ.get('RUN_LLVM_LINK_TESTS') != '1':
        print("Skipping execution test. Set RUN_LLVM_LINK_TESTS=1 to run.")
        return
        
    cpfe_path = 'build/test_llvm_enabled/bin/cpfe'
    if not os.path.exists(cpfe_path):
        print(f"FAIL: {cpfe_path} not found. Please build cpfe first.")
        sys.exit(1)
        
    test_file = 'tests/dummy_test_file.cpp'
    with open(test_file, 'w') as f:
        f.write("int main() { return 0; }\n")
        
    out_file = 'tests/dummy_test_file.ll'
        
    print(f"Running: {cpfe_path} --gen_llvm_file_name {out_file} {test_file}")
    result = subprocess.run(
        [cpfe_path, '--gen_llvm_file_name', out_file, test_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    # We just want to ensure it doesn't crash from unrecognized flag.
    # It might fail if the environment isn't right, but we look for flag parsing success.
    if result.returncode < 0:
        print("FAIL: cpfe crashed.")
        print(result.stderr)
        sys.exit(1)
        
    if "unrecognized option" in result.stderr.lower() and "gen_llvm_file_name" in result.stderr:
        print("FAIL: cpfe did not recognize the --gen_llvm_file_name flag.")
        print(result.stderr)
        sys.exit(1)
        
    print("PASS: cpfe accepted --gen_llvm_file_name flag without crashing.")
    
    if os.path.exists(test_file):
        os.remove(test_file)
    if os.path.exists(out_file):
        os.remove(out_file)

if __name__ == '__main__':
    test_driver_flag()
