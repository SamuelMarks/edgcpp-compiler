# CMakeLists.txt for gcc_be tests

add_custom_target(run_gcc_be_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running GCC Backend tests (Mocked for now)"
)

add_test(NAME test_gcc_be COMMAND ${CMAKE_COMMAND} -E echo "Testing...")
set_tests_properties(test_gcc_be PROPERTIES FIXTURES_SETUP "gcc_be_test_run")

add_test(NAME test_gcc_be_coverage COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_coverage.py)
set_tests_properties(test_gcc_be_coverage PROPERTIES FIXTURES_REQUIRED "gcc_be_test_run")

add_custom_target(coverage_gcc_be
    COMMAND ${CMAKE_CTEST_COMMAND} -R test_gcc_be_coverage --output-on-failure
)
