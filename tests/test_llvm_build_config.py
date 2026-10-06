#!/usr/bin/env python3
import subprocess
import os
import shutil
import sys

def run_cmake(build_dir, args):
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir)
    
    cmd = ['cmake', '../../'] + args
    return subprocess.run(
        cmd,
        cwd=build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

def test_missing_llvm_fails_gracefully():
    print("Testing missing LLVM configuration...")
    result = run_cmake('build/test_llvm_missing', [
        '-DENABLE_LLVM_BACKEND=TRUE', 
        '-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE'
    ])
    expected_msg = "LLVM IR backend was enabled (ENABLE_LLVM_BACKEND=TRUE), but LLVM was not"
    if result.returncode != 0 and expected_msg in result.stderr:
        print("PASS: CMake failed gracefully with the expected error.")
    else:
        print("FAIL: CMake did not fail gracefully or output the expected error.")
        sys.exit(1)

def test_cpfe_links_without_llvm():
    # Only run full build checks if explicitly requested or in a CI environment
    if os.environ.get('RUN_LLVM_LINK_TESTS') != '1':
        print("Skipping link tests. Set RUN_LLVM_LINK_TESTS=1 to run.")
        return
        
    print("Testing build without LLVM...")
    result = run_cmake('build/test_llvm_disabled', [
        '-DENABLE_LLVM_BACKEND=FALSE'
    ])
    if result.returncode != 0:
        print("FAIL: CMake config failed for LLVM disabled.")
        sys.exit(1)
        
    build_result = subprocess.run(
        ['cmake', '--build', 'build/test_llvm_disabled', '--target', 'cpfe'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    if build_result.returncode == 0:
        print("PASS: cpfe links successfully without LLVM.")
    else:
        print("FAIL: cpfe failed to link without LLVM.")
        print(build_result.stderr)
        sys.exit(1)

def test_cpfe_links_with_llvm():
    if os.environ.get('RUN_LLVM_LINK_TESTS') != '1':
        return
        
    print("Testing build with LLVM...")
    result = run_cmake('build/test_llvm_enabled', [
        '-DENABLE_LLVM_BACKEND=TRUE'
    ])
    if result.returncode != 0:
        print("FAIL: CMake config failed for LLVM enabled.")
        sys.exit(1)
        
    build_result = subprocess.run(
        ['cmake', '--build', 'build/test_llvm_enabled', '--target', 'cpfe'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    if build_result.returncode == 0:
        print("PASS: cpfe links successfully with LLVM.")
    else:
        print("FAIL: cpfe failed to link with LLVM.")
        print(build_result.stderr)
        sys.exit(1)

if __name__ == '__main__':
    test_missing_llvm_fails_gracefully()
    test_cpfe_links_without_llvm()
    test_cpfe_links_with_llvm()
    print("All Phase 1 tests passed!")
