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
