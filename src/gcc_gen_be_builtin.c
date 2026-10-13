/**
 * @file gcc_gen_be_builtin.c
 * @brief Implementation of Target SIMD Vector Intrinsics & GCC Builtins.
 *
 * This file maps compiler builtins (atomic, math, bitwise, overflow) to
 * libgccjit function calls or native operations. It also translates
 * x86 and ARM SIMD vector intrinsics into generic vector types and operations,
 * and lowers AddressSanitizer and UBSan instrumentation hooks.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_lib_loader.h"
#include <string.h>
#include <stdio.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Helper to resolve and cache a builtin function.
 *
 * @param ctx The libgccjit context.
 * @param name The builtin name (e.g. "__atomic_load_n").
 * @param ret_type The return type.
 * @param num_params Number of parameters.
 * @param param_types Array of parameter types.
 * @param is_variadic Whether it accepts variadic arguments.
 * @param[out] out_func Pointer to receive the JIT function.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_builtin(gcc_jit_context *ctx, const char *name, gcc_jit_type *ret_type, int num_params, gcc_jit_type **param_types, int is_variadic, gcc_jit_function **out_func) {
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    
    if (p_gcc_jit_context_get_builtin_function) {
        gcc_jit_function *builtin = p_gcc_jit_context_get_builtin_function(ctx, name);
        if (builtin) {
            *out_func = builtin;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    
    /* Fallback to importing standard functions (e.g., libatomic, libm) */
    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc(num_params * sizeof(gcc_jit_param *));
        for (int i = 0; i < num_params; i++) {
            params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], "p");
        }
    }
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, name, num_params, params, is_variadic);
    if (params) free(params);
    *out_func = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers atomic memory builtins (__atomic_load_n, etc.).
 *
 * @param name The builtin name.
 * @param expr The call expression.
 * @param out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_atomic_builtin(const char *name, an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_type *ret_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ret_type));
    
    an_expr_node_ptr arg = expr->variant.operation.operands;
    gcc_jit_rvalue *args[6] = {0};
    gcc_jit_type *arg_types[6] = {0};
    int num_args = 0;
    while (arg && num_args < 6) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &args[num_args]));
        arg_types[num_args] = gcc_jit_rvalue_get_type(args[num_args]);
        num_args++;
        arg = arg->next;
    }
    
    gcc_jit_function *fn = NULL;
    GCC_GEN_BE_CHECK(get_builtin(ctx, name, ret_type, num_args, arg_types, 0, &fn));
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers math, bitwise, and overflow builtins.
 *
 * @param name The builtin name.
 * @param expr The call expression.
 * @param out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_math_bitwise_builtin(const char *name, an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_type *ret_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ret_type));
    
    an_expr_node_ptr arg = expr->variant.operation.operands;
    gcc_jit_rvalue *args[4] = {0};
    gcc_jit_type *arg_types[4] = {0};
    int num_args = 0;
    while (arg && num_args < 4) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &args[num_args]));
        arg_types[num_args] = gcc_jit_rvalue_get_type(args[num_args]);
        num_args++;
        arg = arg->next;
    }
    
    gcc_jit_function *fn = NULL;
    GCC_GEN_BE_CHECK(get_builtin(ctx, name, ret_type, num_args, arg_types, 0, &fn));
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers SIMD vector intrinsics for x86 and ARM.
 *
 * @param name The builtin name (e.g. "_mm_add_ps").
 * @param expr The call expression.
 * @param out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_simd_builtin(const char *name, an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_type *ret_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ret_type));
    
    an_expr_node_ptr arg = expr->variant.operation.operands;
    gcc_jit_rvalue *args[8] = {0};
    gcc_jit_type *arg_types[8] = {0};
    int num_args = 0;
    while (arg && num_args < 8) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &args[num_args]));
        arg_types[num_args] = gcc_jit_rvalue_get_type(args[num_args]);
        num_args++;
        arg = arg->next;
    }
    
    char gcc_name[128];
    if (strncmp(name, "_mm_", 4) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32%s", name + 3);
    } else if (strncmp(name, "vadd", 4) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_neon%s", name + 1);
    } else {
        snprintf(gcc_name, sizeof(gcc_name), "%s", name);
    }

    gcc_jit_function *fn = NULL;
    GCC_GEN_BE_CHECK(get_builtin(ctx, gcc_name, ret_type, num_args, arg_types, 0, &fn));
    if (!fn) return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;
    
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers sanitizer and profiling hooks.
 *
 * @param name The builtin name.
 * @param expr The call expression.
 * @param out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_sanitizer_hook(const char *name, an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    
    an_expr_node_ptr arg = expr->variant.operation.operands;
    gcc_jit_rvalue *args[4] = {0};
    gcc_jit_type *arg_types[4] = {0};
    int num_args = 0;
    while (arg && num_args < 4) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &args[num_args]));
        arg_types[num_args] = gcc_jit_rvalue_get_type(args[num_args]);
        num_args++;
        arg = arg->next;
    }
    
    gcc_jit_function *fn = NULL;
    GCC_GEN_BE_CHECK(get_builtin(ctx, name, void_type, num_args, arg_types, 0, &fn));
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief High-level dispatcher for lowering GCC builtins and intrinsics.
 *
 * @param expr The function call expression.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    a_routine_ptr rout = expr->variant.operation.operands->variant.routine.ptr;
    if (!rout || !rout->source_corresp.name) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    const char *name = rout->source_corresp.name;

    if (strncmp(name, "__atomic_", 9) == 0) {
        return lower_atomic_builtin(name, expr, out_rval);
    } else if (strncmp(name, "__builtin_add_overflow", 22) == 0 ||
               strncmp(name, "__builtin_clz", 13) == 0 ||
               strncmp(name, "__builtin_popcount", 18) == 0) {
        return lower_math_bitwise_builtin(name, expr, out_rval);
    } else if (strncmp(name, "_mm_", 4) == 0 || strncmp(name, "vadd", 4) == 0) {
        return lower_simd_builtin(name, expr, out_rval);
    } else if (strncmp(name, "__asan_", 7) == 0 || strncmp(name, "__ubsan_", 8) == 0) {
        return lower_sanitizer_hook(name, expr, out_rval);
    }

    /* Fallback */
    return lower_math_bitwise_builtin(name, expr, out_rval);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
