/**
 * @file gcc_gen_be_builtin.c
 * @brief Implementation of Builtin and Intrinsic lowering.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_lib_loader.h"
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    const char *name = rout->source_corresp.name;
    if (!name) return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;

    /* Use libgccjit's native builtin resolver if possible */
    gcc_jit_function *builtin_fn = p_gcc_jit_context_get_builtin_function ? p_gcc_jit_context_get_builtin_function(ctx, name) : NULL;
    if (builtin_fn) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        
        gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, builtin_fn, num_args, args);
        *out_rval = call;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Handle custom EDG-specific builtins or those unsupported directly by gcc_jit_context_get_builtin_function */
    if (strncmp(name, "__atomic_", 9) == 0 || strncmp(name, "__sync_", 7) == 0) {
        /* Atomic Memory Model operations */
        /* For Phase 6 completeness without native atomic primitives in libgccjit,
           we construct function imports matching the standard GCC libatomic ABI. */
        
        gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
        
        gcc_jit_function *libatomic_fn = NULL;
        
        if (strcmp(name, "__atomic_load") == 0) {
            gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), "size");
            gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
            gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ret");
            gcc_jit_param *p4 = gcc_jit_context_new_param(ctx, NULL, int_type, "model");
            gcc_jit_param *params[] = {p1, p2, p3, p4};
            libatomic_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__atomic_load", 4, params, 0);
        } else if (strcmp(name, "__atomic_store") == 0) {
            gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), "size");
            gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
            gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "val");
            gcc_jit_param *p4 = gcc_jit_context_new_param(ctx, NULL, int_type, "model");
            gcc_jit_param *params[] = {p1, p2, p3, p4};
            libatomic_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__atomic_store", 4, params, 0);
        } else if (strcmp(name, "__atomic_compare_exchange") == 0) {
            gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), "size");
            gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
            gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "expected");
            gcc_jit_param *p4 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "desired");
            gcc_jit_param *p5 = gcc_jit_context_new_param(ctx, NULL, int_type, "succ");
            gcc_jit_param *p6 = gcc_jit_context_new_param(ctx, NULL, int_type, "fail");
            gcc_jit_param *params[] = {p1, p2, p3, p4, p5, p6};
            libatomic_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, bool_type, "__atomic_compare_exchange", 6, params, 0);
        } else {
            /* Create a generic varargs imported function for other atomic builtins */
            gcc_jit_type *ret_type = NULL;
            err = gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &ret_type);
            if (!ret_type || err != GCC_GEN_BE_SUCCESS) ret_type = void_ptr_type;
            libatomic_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, name, 0, NULL, 1);
        }
        
        if (libatomic_fn) {
            gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, libatomic_fn, num_args, args);
            *out_rval = call;
            return GCC_GEN_BE_SUCCESS;
        }
    }

    return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
