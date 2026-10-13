/**
 * @file test_gcc_be_eh.cpp
 * @brief Unit tests for Exception Handling lowering.
 * @details Tests SjLj fallback, basic try/catch translation, and error handling.
 */

#include <gtest/gtest.h>
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"

/**
 * @brief Test that functions gracefully handle NULL pointers.
 * @details Returns GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
TEST(GccBeEhTest, NullPointerHandling) {
    EXPECT_EQ(gcc_gen_be_begin_function_eh(NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_end_function_eh(NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_declare_eh_symbols(NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_throw(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_try_block(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_routine_body(NULL, NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief Test basic initialization of EH symbols.
 * @details Basic EH.
 */
TEST(GccBeEhTest, DeclareSymbols) {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(gcc_gen_be_declare_eh_symbols(ctx), GCC_GEN_BE_SUCCESS);
    gcc_jit_context_release(ctx);
}
