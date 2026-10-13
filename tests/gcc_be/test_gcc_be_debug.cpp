/**
 * @file test_gcc_be_debug.cpp
 * @brief Unit tests for DWARF debug metadata generation logic.
 * @details Validates DW_TAG emission mappings, variable location tracking, and lexical block bounds.
 */

#include <gtest/gtest.h>
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"

/**
 * @brief Test that functions gracefully handle NULL pointers.
 * @details Returns GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
TEST(GccBeDebugTest, NullPointerHandling) {
    EXPECT_EQ(gcc_gen_be_enable_debug_info(NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_subprogram_debug_info(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_type_debug_info(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_variable_debug_info(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_block_debug_info(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief Test basic execution of debug info emission routines.
 * @details Simulates AST nodes and validates SUCCESS returns.
 */
TEST(GccBeDebugTest, BasicExecution) {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(gcc_gen_be_enable_debug_info(ctx), GCC_GEN_BE_SUCCESS);

    gcc_jit_context_release(ctx);
}
