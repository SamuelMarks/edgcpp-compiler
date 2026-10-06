#!/bin/bash
set -e

# Build with coverage
cmake -S . -B build/coverage -DBUILD_TESTING=ON -DENABLE_LLVM_BACKEND=ON -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_C_FLAGS="--coverage"
cmake --build build/coverage

# Run tests
cd build/coverage
ctest -V

# Generate coverage
lcov --capture --directory src/CMakeFiles/cpfe.dir --output-file coverage.info || echo "lcov failed but tests passed"
lcov --extract coverage.info '*/src/llvm_gen_be*.cpp' --output-file llvm_be_coverage.info || echo "lcov extract failed"
genhtml llvm_be_coverage.info --output-directory coverage_html || echo "genhtml failed"

echo "Coverage report generated in build/coverage/coverage_html"

# Enforce 100% line coverage for specific LLVM backend files
coverage_files=("src/llvm_gen_be_stmt.cpp" "src/llvm_gen_be_expr.cpp" "src/llvm_gen_be_type.cpp" "src/llvm_gen_be_const.cpp")
for file in "${coverage_files[@]}"; do
    if ! lcov --list llvm_be_coverage.info | grep "$file" | grep -q "100.0%"; then
        echo "Error: $file does not have 100% test coverage."
        exit 1
    fi
done
