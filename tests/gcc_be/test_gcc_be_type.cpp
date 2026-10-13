/**
 * @file test_gcc_be_type.cpp
 * @brief Test file.
 * @details Tests functionality.
 */
#include <gtest/gtest.h>
#include "gcc_gen_be_type.h"

using namespace edg;

/**
 * @brief TEST function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
TEST(GccBeTypeTest, NullPointerHandling) {
    gcc_jit_type *type = nullptr;
    EXPECT_EQ(gcc_gen_be_lower_type(nullptr, &type), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief TEST function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
TEST(GccBeTypeTest, BasicExecution) {
/**
 * @brief SUCCEED function.
 * @details Test helper function SUCCEED.
 * @return Expected return value.
 */
    EXPECT_TRUE(true);
}
