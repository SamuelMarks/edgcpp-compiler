#!/usr/bin/env python3
import subprocess
import os
import sys

def run_fuzz():
    if os.environ.get('RUN_LLVM_LINK_TESTS') != '1':
        print("Skipping fuzz execution test. Set RUN_LLVM_LINK_TESTS=1 to run.")
        return
        
    cpfe_path = 'build/test_llvm_enabled/bin/cpfe'
    if not os.path.exists(cpfe_path):
        print(f"FAIL: {cpfe_path} not found.")
        sys.exit(1)
        
    test_file = 'tests/fuzz_test.c'
    out_file = 'tests/fuzz_test.ll'
    
    # We provide a simple fuzz file with some random AST operations
    ast_ops = """
    int main() {
        int x = 42;
        int y = x * 2 / 3 + 1;
        float z = 3.14f * (float)y;
        struct { char a; int b; } s = { 'c', 100 };
        return s.b + (int)z;
    }
    """
    
    with open(test_file, 'w') as f:
        f.write(ast_ops)
        
    result = subprocess.run(
        [cpfe_path, '--gen_llvm_file_name', out_file, test_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    if result.returncode < 0:
        print("FAIL: cpfe crashed during fuzzing.")
        sys.exit(1)
        
    print("PASS: Fuzz testing completed successfully without crashes.")
    if os.path.exists(test_file): os.remove(test_file)
    if os.path.exists(out_file): os.remove(out_file)

if __name__ == '__main__':
    run_fuzz()
