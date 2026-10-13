/**
 * @file test_gcc_be_location.cpp
 * @brief Unit tests for Source Location Tracking.
 * @details Tests AST-to-location mapping with line and column coordinates.
 */

#include <gtest/gtest.h>
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
#include "il.h"

/**
 * @brief Test that functions gracefully handle NULL pointers.
 * @details Returns GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
TEST(GccBeLocationTest, NullPointerHandling) {
    EXPECT_EQ(gcc_gen_be_get_location_from_type(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_get_location_from_var(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_get_location_from_routine(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_get_location_from_expr(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_get_location_from_stmt(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief Test basic location mapping execution.
 * @details Validates success when given NULL nodes but valid output pointers.
 */
TEST(GccBeLocationTest, BasicExecution) {
    struct gcc_jit_location *loc = NULL;
    EXPECT_EQ(gcc_gen_be_get_location_from_type(NULL, &loc), GCC_GEN_BE_SUCCESS);
    EXPECT_EQ(gcc_gen_be_get_location_from_var(NULL, &loc), GCC_GEN_BE_SUCCESS);
    EXPECT_EQ(gcc_gen_be_get_location_from_routine(NULL, &loc), GCC_GEN_BE_SUCCESS);
    EXPECT_EQ(gcc_gen_be_get_location_from_expr(NULL, &loc), GCC_GEN_BE_SUCCESS);
    EXPECT_EQ(gcc_gen_be_get_location_from_stmt(NULL, &loc), GCC_GEN_BE_SUCCESS);
}
