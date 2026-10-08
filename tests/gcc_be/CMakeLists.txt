# CMakeLists.txt for gcc_be tests

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
    # Determine if it's C or C++
    if(test_file MATCHES "\\.cpp$")
        set(TEST_LANG "CXX")
    else()
        set(TEST_LANG "C")
    endif()

    add_test(NAME test_gcc_be_${test_file} 
             COMMAND $<TARGET_FILE:edgcpfe> --gcc_gen_be -c ${CMAKE_CURRENT_SOURCE_DIR}/${test_file} -o ${CMAKE_CURRENT_BINARY_DIR}/${test_file}.o)
    set_tests_properties(test_gcc_be_${test_file} PROPERTIES FIXTURES_SETUP "gcc_be_test_run")
endforeach()

add_test(NAME test_gcc_be_coverage COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_coverage.py)
set_tests_properties(test_gcc_be_coverage PROPERTIES FIXTURES_REQUIRED "gcc_be_test_run")

add_custom_target(coverage_gcc_be
    COMMAND ${CMAKE_CTEST_COMMAND} -R test_gcc_be_coverage --output-on-failure
)
