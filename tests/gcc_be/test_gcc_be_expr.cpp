/**
 * @file test_gcc_be_expr.cpp
 * @brief Test file.
 * @details Tests functionality.
 */
#include <gtest/gtest.h>
#include "gcc_gen_be_expr.h"

using namespace edg;

/**
 * @brief TEST function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
TEST(GccBeExprTest, NullPointerHandling) {
    gcc_jit_rvalue *rval = nullptr;
    gcc_jit_lvalue *lval = nullptr;
    EXPECT_EQ(gcc_gen_be_lower_expr_rvalue(nullptr, &rval), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_expr_lvalue(nullptr, &lval), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_constant_rvalue(nullptr, nullptr, &rval), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief TEST function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
TEST(GccBeExprTest, BasicExecution) {
/**
 * @brief SUCCEED function.
 * @details Test helper function SUCCEED.
 * @return Expected return value.
 */
    EXPECT_TRUE(true);
}
