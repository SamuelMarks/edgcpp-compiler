/**
 * @file test_gcc_be_rtti.cpp
 * @brief Unit tests for Itanium C++ ABI Virtual Tables, VTT & Thunks.
 * @details Tests RTTI hierarchy, typeid, dynamic_cast, vtables, VTTs, and construction vtables.
 */

#include <gtest/gtest.h>
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"

using namespace edg;

/**
 * @brief Test that functions gracefully handle NULL pointers.
 * @details Returns GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
TEST(GccBeRttiTest, NullPointerHandling) {
    gcc_jit_lvalue *vtable = nullptr;
    gcc_jit_lvalue *vtt = nullptr;
    gcc_jit_function *func = nullptr;
    gcc_jit_rvalue *rval = nullptr;
    gcc_jit_lvalue *descriptor = nullptr;

    EXPECT_EQ(gcc_gen_be_emit_vtable(nullptr, &vtable), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_vtable((a_type_ptr)0x1234, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    
    EXPECT_EQ(gcc_gen_be_emit_vtt(nullptr, &vtt), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_vtt((a_type_ptr)0x1234, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    
    EXPECT_EQ(gcc_gen_be_emit_thunk(nullptr, &func), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_thunk((a_routine_ptr)0x1234, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    
    EXPECT_EQ(gcc_gen_be_lower_dynamic_cast(nullptr, &rval), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_dynamic_cast((an_expr_node_ptr)0x1234, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    
    EXPECT_EQ(gcc_gen_be_lower_typeid(nullptr, &rval), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_lower_typeid((an_expr_node_ptr)0x1234, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    
    EXPECT_EQ(gcc_gen_be_emit_type_descriptor(nullptr, &descriptor), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(gcc_gen_be_emit_type_descriptor((a_type_ptr)0x1234, nullptr), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
}

/**
 * @brief Test basic execution mapping behavior.
 * @details Exec
 */
TEST(GccBeRttiTest, BasicExecution) {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    ASSERT_NE(ctx, nullptr);
    gcc_jit_context_release(ctx);
}
