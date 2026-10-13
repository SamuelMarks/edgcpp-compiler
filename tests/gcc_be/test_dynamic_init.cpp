/**
 * @file test_dynamic_init.cpp
 * @brief Test file.
 * @details Tests functionality.
 */
#include <stdio.h>
#include <stdlib.h>

/** @brief my_global member. */
int my_global = 0;

/**
 * @brief GlobalInitializer struct.
 * @details Helper struct for testing.
 */
struct GlobalInitializer {
/**
 * @brief GlobalInitializer function.
 * @details Test helper function GlobalInitializer.
 * @return Expected return value.
 */
    GlobalInitializer() {
        my_global = 42;
    }
};

/** @brief instance member. */
GlobalInitializer instance;

/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    if (my_global == 42) {
/**
 * @brief printf function.
 * @details Test helper function printf.
 * @param[in] PASS\n" Input parameter.
 * @return Expected return value.
 */
        printf("Dynamic Init PASS\n");
        return 0;
    } else {
/**
 * @brief printf function.
 * @details Test helper function printf.
 * @param[in] %d\n" Input parameter.
 * @param[in] my_global Input parameter.
 * @return Expected return value.
 */
        printf("Dynamic Init FAIL: %d\n", my_global);
        return 1;
    }
}
