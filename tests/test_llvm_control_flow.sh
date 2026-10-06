#!/bin/bash
set -e

if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping control flow execution test. Set RUN_LLVM_LINK_TESTS=1 to run."
    exit 0
fi

CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_C="tests/control_flow_test.c"
TEST_LL="tests/control_flow_test.ll"

cat << 'C_EOF' > "$TEST_C"
int test_control_flow(int a) {
    if (a > 10) {
        a = a + 1;
    } else {
        a = a - 1;
    }
    
    while (a > 0) {
        a = a - 1;
        if (a == 5) break;
    }
    
    switch (a) {
        case 1: return 100;
        case 2: return 200;
        default: return 300;
    }
    return a;
}
C_EOF

echo "Running $CPFE_PATH on control_flow_test.c..."
# cpfe will call llvm::verifyModule internally. If it fails due to un-terminated blocks,
# cpfe should exit with an error.
$CPFE_PATH --gen_llvm_file_name "$TEST_LL" "$TEST_C" || {
    echo "FAIL: cpfe execution failed. Basic blocks might be un-terminated, failing llvm::verifyModule."
    exit 1
}

# Also ensure we actually generated the branch instructions.
if ! grep -q "br " "$TEST_LL"; then
    echo "FAIL: Missing branch (br) instructions."
    exit 1
fi

if ! grep -q "switch " "$TEST_LL"; then
    echo "FAIL: Missing switch instruction."
    exit 1
fi

echo "PASS: Control flow tests passed (no un-terminated blocks)."

rm "$TEST_C" "$TEST_LL"
