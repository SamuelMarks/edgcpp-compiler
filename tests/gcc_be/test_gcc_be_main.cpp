/**
 * @file test_gcc_be_main.cpp
 * @brief Unit tests for compiler driver and orchestration.
 * @details Tests multi-TU compilation, artifact emission, and command-line parsing.
 */

#include <gtest/gtest.h>
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"

/**
 * @brief Test that gcc_gen_be_early_init succeeds.
 * @details Validates libgccjit loading.
 */
TEST(GccBeMainTest, EarlyInit) {
    EXPECT_EQ(gcc_gen_be_early_init(), GCC_GEN_BE_SUCCESS);
}

/**
 * @brief Test that functions gracefully handle NULL pointers.
 * @details Returns GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
TEST(GccBeMainTest, NullPointerHandling) {
    EXPECT_EQ(gcc_gen_be_apply_command_line_options(NULL, 0, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_compile_multi_tu(NULL, 0, 0, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_artifact(NULL, "obj", "out.o"), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(gcc_gen_be_emit_artifact(ctx, NULL, "out.o"), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_artifact(ctx, "obj", NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    gcc_jit_context_release(ctx);
}

/**
 * @brief Test command line options application.
 * @details Exercises optimization flags, -g, -fPIC, -march.
 */
TEST(GccBeMainTest, ApplyCommandLineOptions) {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    ASSERT_NE(ctx, nullptr);

    const char *argv[] = {"-O3", "-g", "-fPIC", "-march=x86-64"};
    EXPECT_EQ(gcc_gen_be_apply_command_line_options(ctx, 4, (char**)argv), GCC_GEN_BE_SUCCESS);

    gcc_jit_context_release(ctx);
}
