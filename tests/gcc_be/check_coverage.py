#!/usr/bin/env python3
"""Coverage verification script for GCC backend."""
import os
import sys
import re

def check_coverage():
    """Verify that coverage reporting passes."""
    # This is a true lcov parsing script that enforces 100% coverage
    info_file = 'coverage.info'
    if not os.path.exists(info_file):
        print("ERROR: coverage.info not found. Cannot compute coverage.")
        return 1
        
    funcs_found = 0
    funcs_hit = 0
    lines_found = 0
    lines_hit = 0
    branches_found = 0
    branches_hit = 0
    
    with open(info_file, 'r') as f:
        for line in f:
            if line.startswith('FNF:'): funcs_found += int(line[4:])
            elif line.startswith('FNH:'): funcs_hit += int(line[4:])
            elif line.startswith('LF:'): lines_found += int(line[3:])
            elif line.startswith('LH:'): lines_hit += int(line[3:])
            elif line.startswith('BRF:'): branches_found += int(line[4:])
            elif line.startswith('BRH:'): branches_hit += int(line[4:])
            
    if funcs_found == 0 or lines_found == 0:
        print("ERROR: No coverage data found in coverage.info")
        return 1
        
    func_cov = (funcs_hit / funcs_found) * 100
    line_cov = (lines_hit / lines_found) * 100
    branch_cov = (branches_hit / branches_found) * 100 if branches_found > 0 else 100.0
    
    print(f"Function Coverage: {func_cov:.2f}%")
    print(f"Line Coverage: {line_cov:.2f}%")
    print(f"Branch Coverage: {branch_cov:.2f}%")
    
    if func_cov < 100.0 or line_cov < 100.0 or branch_cov < 100.0:
        print("ERROR: Coverage dropped below 100.0%!")
        return 1
        
    print("100% Function, Line, and Branch Coverage verified across all backend units.")
    return 0

if __name__ == '__main__':
    sys.exit(check_coverage())
