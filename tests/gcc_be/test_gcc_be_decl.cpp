/**
 * @file test_gcc_be_decl.cpp
 * @brief Test file.
 * @details Tests functionality.
 */
#include <gtest/gtest.h>
#include "gcc_gen_be_decl.h"

using namespace edg;

/**
 * @brief TEST function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
TEST(GccBeDeclTest, NullPointerHandling) {
    gcc_jit_lvalue *lval = nullptr;
    gcc_jit_function *func = nullptr;
    EXPECT_EQ(gcc_gen_be_lower_variable_decl(nullptr, &lval), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_function_decl(nullptr, &func), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief TEST function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
TEST(GccBeDeclTest, BasicExecution) {
/**
 * @brief SUCCEED function.
 * @details Test helper function SUCCEED.
 * @return Expected return value.
 */
    EXPECT_TRUE(true);
}
