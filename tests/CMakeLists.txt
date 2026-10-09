# Test configuration

include(CTest)

if(ENABLE_LLVM_BACKEND)
  if(APPLE)
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
      execute_process(
        COMMAND ${BREW_EXECUTABLE} --prefix llvm
        OUTPUT_VARIABLE BREW_LLVM_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(EXISTS "${BREW_LLVM_PREFIX}")
        list(APPEND CMAKE_PREFIX_PATH "${BREW_LLVM_PREFIX}")
        set(LLVM_DIR "${BREW_LLVM_PREFIX}/lib/cmake/llvm" CACHE PATH "LLVM config dir" FORCE)
      endif()
    endif()
  endif()
  find_package(LLVM CONFIG REQUIRED)
endif()

add_executable(eh_test llvm_ir_backend_tests/eh_test.cpp)
add_executable(const_test llvm_ir_backend_tests/const_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_const.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(const_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(const_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(const_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(const_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)
add_executable(asm_test llvm_ir_backend_tests/asm_test.cpp)
add_executable(goto_test llvm_ir_backend_tests/goto_test.cpp)
add_executable(error_test llvm_ir_backend_tests/error_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(error_test PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(type_test llvm_ir_backend_tests/type_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(type_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(type_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(type_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(type_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)

add_test(NAME eh_test COMMAND eh_test)
add_test(NAME const_test COMMAND const_test)
add_test(NAME asm_test COMMAND asm_test)
add_test(NAME goto_test COMMAND goto_test)
add_test(NAME error_test COMMAND error_test)
add_test(NAME type_test COMMAND type_test)

add_executable(test_llvm_orchestrator test_llvm_orchestrator.cpp)
target_compile_features(test_llvm_orchestrator PRIVATE cxx_std_17)
target_include_directories(test_llvm_orchestrator PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(test_llvm_orchestrator PRIVATE ${LLVM_LIBS})

add_executable(test_llvm_verifier test_llvm_verifier.cpp)
target_compile_features(test_llvm_verifier PRIVATE cxx_std_17)
target_include_directories(test_llvm_verifier PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(test_llvm_verifier PRIVATE ${LLVM_LIBS})


add_test(NAME test_llvm_verifier COMMAND test_llvm_verifier)

add_executable(abi_win64_test llvm_ir_backend_tests/abi_win64_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_abi_win64.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(abi_win64_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(abi_win64_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(abi_win64_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(abi_win64_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)
add_test(NAME abi_win64_test COMMAND abi_win64_test)

add_executable(debug_init_test llvm_ir_backend_tests/debug_init_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(debug_init_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(debug_init_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(debug_init_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(debug_init_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)
add_test(NAME debug_init_test COMMAND debug_init_test)

add_executable(debug_scope_test llvm_ir_backend_tests/debug_scope_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug_scopes.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(debug_scope_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(debug_scope_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(debug_scope_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(debug_scope_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)
add_test(NAME debug_scope_test COMMAND debug_scope_test)

add_executable(debug_types_test llvm_ir_backend_tests/debug_types_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug_scopes.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_debug_types.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(debug_types_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(debug_types_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(debug_types_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(debug_types_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis)
add_test(NAME debug_types_test COMMAND debug_types_test)

add_executable(opt_pipeline_test llvm_ir_backend_tests/opt_pipeline_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_opt.cpp)
target_include_directories(opt_pipeline_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(opt_pipeline_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(opt_pipeline_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(opt_pipeline_test PRIVATE LLVMCore LLVMSupport LLVMTarget LLVMBitWriter LLVMAnalysis LLVMPasses)
add_test(NAME opt_pipeline_test COMMAND opt_pipeline_test)

add_executable(codegen_test llvm_ir_backend_tests/codegen_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_codegen.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(codegen_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(codegen_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(codegen_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
llvm_map_components_to_libnames(CODEGEN_TEST_LLVM_LIBS Core Support Target BitWriter Analysis Passes MC MCParser TargetParser AsmPrinter AsmParser ${LLVM_TARGETS_TO_BUILD})
target_link_libraries(codegen_test PRIVATE ${CODEGEN_TEST_LLVM_LIBS})
add_test(NAME codegen_test COMMAND codegen_test)

add_executable(fault_injection_test llvm_ir_backend_tests/fault_injection_test.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_type.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_expr.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_codegen.cpp ${CMAKE_SOURCE_DIR}/src/llvm_gen_be_error.cpp)
target_include_directories(fault_injection_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src/includes ${LLVM_INCLUDE_DIRS})
target_compile_definitions(fault_injection_test PRIVATE BACK_END_IS_LLVM_GEN_BE=1)
set_target_properties(fault_injection_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(fault_injection_test PRIVATE ${CODEGEN_TEST_LLVM_LIBS})
add_test(NAME fault_injection_test COMMAND fault_injection_test)
