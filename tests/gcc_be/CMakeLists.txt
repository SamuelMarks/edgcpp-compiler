# CMakeLists.txt for gcc_be tests

option(EDG_GCC_BE_ENABLE_COVERAGE "Enable code coverage flags for GCC backend" OFF)
if(EDG_GCC_BE_ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

set(GCC_BE_TEST_FILES
    test_scalars.c
    test_types.c
    test_expr_ops.c
    test_short_circuit.c
    test_control_flow.c
    test_functions.c
    test_aggregates.c
    test_cpp_classes.cpp
    test_virtual.cpp
    test_eh.cpp
    test_inline_asm.c
    test_edge_cases.c
)

add_custom_target(run_gcc_be_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running GCC Backend tests"
)

foreach(test_file ${GCC_BE_TEST_FILES})
    set(EXTRA_FLAGS "")
    if(test_file STREQUAL "test_virtual.cpp")
        set(EXTRA_FLAGS "--c++11")
    elseif(test_file STREQUAL "test_eh.cpp")
        set(EXTRA_FLAGS "--c++" "--exceptions")
    elseif(test_file MATCHES "\\.cpp$")
        set(EXTRA_FLAGS "--c++")
    elseif(test_file STREQUAL "test_aggregates.c")
        set(EXTRA_FLAGS "--c99")
    elseif(test_file STREQUAL "test_edge_cases.c")
        set(EXTRA_FLAGS "--gcc")
    endif()

    add_test(NAME test_gcc_be_${test_file} 
             COMMAND $<TARGET_FILE:cpfe> --gcc_gen_be ${EXTRA_FLAGS} -c -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o ${CMAKE_CURRENT_SOURCE_DIR}/${test_file})
    set_tests_properties(test_gcc_be_${test_file} PROPERTIES 
        FIXTURES_SETUP "gcc_be_test_run"
        ENVIRONMENT "EDG_BASE=${PROJECT_SOURCE_DIR}/bases/cmake-native/macos-arm/clang")
endforeach()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME test_gcc_be_coverage COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/check_coverage.py)
set_tests_properties(test_gcc_be_coverage PROPERTIES FIXTURES_REQUIRED "gcc_be_test_run")

add_test(NAME test_gcc_be_doc_coverage COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/dev_tools/check_gcc_be_doc_coverage.py)

add_custom_target(coverage_gcc_be
    COMMAND ${CMAKE_CTEST_COMMAND} -R test_gcc_be_coverage --output-on-failure
)

# Unit tests for backend components
add_executable(test_gcc_be_errors test_gcc_be_errors.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_error.c)
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_errors_unit COMMAND test_gcc_be_errors)
target_compile_options(test_gcc_be_errors PRIVATE "-x" "c++")
target_compile_definitions(test_gcc_be_errors PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_include_directories(test_gcc_be_errors PRIVATE ${PROJECT_BINARY_DIR}/includes)
add_dependencies(test_gcc_be_errors error-info)

add_executable(test_gcc_be_context test_gcc_be_context.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_gcc_be_context PRIVATE LibGCCJIT::LibGCCJIT)
add_test(NAME test_gcc_be_context_unit COMMAND test_gcc_be_context)
target_compile_options(test_gcc_be_context PRIVATE "-x" "c++")
target_compile_definitions(test_gcc_be_context PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_include_directories(test_gcc_be_context PRIVATE ${PROJECT_BINARY_DIR}/includes)
add_dependencies(test_gcc_be_context error-info)

add_executable(test_gcc_be_lib_loader test_gcc_be_lib_loader.cpp)
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_lib_loader_unit COMMAND test_gcc_be_lib_loader)
target_compile_definitions(test_gcc_be_lib_loader PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_lib_loader PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_lib_loader PRIVATE ${PROJECT_BINARY_DIR}/includes)

add_executable(test_gcc_be_cache test_gcc_be_cache.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c)
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_cache_unit COMMAND test_gcc_be_cache)
target_compile_definitions(test_gcc_be_cache PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_cache PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_cache PRIVATE ${PROJECT_BINARY_DIR}/includes)

add_executable(test_gcc_be_type test_gcc_be_type.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_type_unit COMMAND test_gcc_be_type)
target_compile_definitions(test_gcc_be_type PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_type PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_type PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_type PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_rtti test_gcc_be_rtti.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_rtti PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_rtti_unit COMMAND test_gcc_be_rtti)
target_compile_definitions(test_gcc_be_rtti PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_rtti PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_rtti PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_rtti PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_builtin test_gcc_be_builtin.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_builtin.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_builtin PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_builtin_unit COMMAND test_gcc_be_builtin)
target_compile_definitions(test_gcc_be_builtin PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_builtin PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_builtin PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_builtin PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_decl test_gcc_be_decl.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_decl.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_type.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c)
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_decl_unit COMMAND test_gcc_be_decl)
target_compile_definitions(test_gcc_be_decl PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_decl PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_decl PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_decl PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_expr test_gcc_be_expr.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_expr_unit COMMAND test_gcc_be_expr)
target_compile_definitions(test_gcc_be_expr PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_expr PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_expr PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_expr PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_abi test_gcc_be_abi.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_abi.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_abi PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_abi_unit COMMAND test_gcc_be_abi)
target_compile_definitions(test_gcc_be_abi PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_abi PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_abi PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_abi PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_debug test_gcc_be_debug.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_debug.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_location.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_debug PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_debug_unit COMMAND test_gcc_be_debug)
target_compile_definitions(test_gcc_be_debug PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_debug PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_debug PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_debug PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_dtor test_gcc_be_dtor.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_dtor.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_dtor PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_dtor_unit COMMAND test_gcc_be_dtor)
target_compile_definitions(test_gcc_be_dtor PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_dtor PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_dtor PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_dtor PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_stmt2 test_gcc_be_stmt2.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_expr.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_stmt2 PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_stmt2_unit COMMAND test_gcc_be_stmt2)
target_compile_definitions(test_gcc_be_stmt2 PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_stmt2 PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_stmt2 PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_stmt2 PRIVATE LibGCCJIT::LibGCCJIT)

add_executable(test_gcc_be_stmt test_gcc_be_stmt.cpp ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_stmt.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_rtti.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_context.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_cache.c ${PROJECT_SOURCE_DIR}/src/gcc_gen_be_lib_loader.c)
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME test_gcc_be_stmt_unit COMMAND test_gcc_be_stmt)
target_compile_definitions(test_gcc_be_stmt PRIVATE BACK_END_IS_GCC_GEN_BE=1)
target_compile_options(test_gcc_be_stmt PRIVATE "-x" "c++")
target_include_directories(test_gcc_be_stmt PRIVATE ${PROJECT_BINARY_DIR}/includes)
target_link_libraries(test_gcc_be_stmt PRIVATE LibGCCJIT::LibGCCJIT)
