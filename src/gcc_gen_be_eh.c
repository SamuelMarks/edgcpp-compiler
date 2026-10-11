/**
 * @file gcc_gen_be_eh.c
 * @brief Implementation of Exception handling and stack unwinding.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Declares an exception handling runtime function.
 *
 * @details Adds the function declaration to the given GCC JIT context.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] name The name of the function.
 * @param[in] ret_type The return type.
 * @param[in] num_params The number of parameters.
 * @param[in] param_types The array of parameter types.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t declare_eh_func(gcc_jit_context *ctx, const char *name, gcc_jit_type *ret_type, int num_params, gcc_jit_type **param_types) {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)name, &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) return GCC_GEN_BE_SUCCESS;

    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc(num_params * sizeof(gcc_jit_param *));
        if (!params) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < num_params; ++i) {
            params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], "p");
        }
    }

    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, name, num_params, params, 0);
    if (params) free(params);

    return cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)name, fn);
}

gcc_gen_be_error_t gcc_gen_be_declare_eh_symbols(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

    /* __gxx_personality_v0 */
    /* Libgccjit doesn't have native personality settings exposed easily,
       but we must declare the symbol. */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gxx_personality_v0", void_type, 0, NULL));

    /* void *__cxa_allocate_exception(size_t thrown_size) */
    gcc_jit_type *p1[] = { size_t_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_allocate_exception", void_ptr_type, 1, p1));

    /* void __cxa_throw(void *thrown_exception, struct std::type_info *tinfo, void (*dest)(void *)) */
    gcc_jit_type *p2[] = { void_ptr_type, void_ptr_type, void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_throw", void_type, 3, p2));

    /* void *__cxa_begin_catch(void *exceptionObject) */
    gcc_jit_type *p3[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_begin_catch", void_ptr_type, 1, p3));

    /* void __cxa_end_catch() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_end_catch", void_type, 0, NULL));

    /* void __cxa_rethrow() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_rethrow", void_type, 0, NULL));

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_throw(an_expr_node_ptr expr, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Phase 3 requires emitting __cxa_allocate_exception and __cxa_throw */
    return GCC_GEN_BE_ERROR_EH_UNSUPPORTED; /* Stub for now */
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
