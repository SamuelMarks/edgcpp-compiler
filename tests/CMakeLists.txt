# Test configuration

include(CTest)

add_executable(eh_test llvm_ir_backend_tests/eh_test.cpp)
add_executable(const_test llvm_ir_backend_tests/const_test.cpp)
add_executable(asm_test llvm_ir_backend_tests/asm_test.cpp)
add_executable(goto_test llvm_ir_backend_tests/goto_test.cpp)

add_test(NAME eh_test COMMAND eh_test)
add_test(NAME const_test COMMAND const_test)
add_test(NAME asm_test COMMAND asm_test)
add_test(NAME goto_test COMMAND goto_test)
