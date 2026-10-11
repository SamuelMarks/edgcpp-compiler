/**
 * @file gcc_gen_be_abi.c
 * @brief Implementation of ABI attributes and calling conventions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
#include "il_def.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    an_attribute_ptr attr = rout->source_corresp.attributes;
    while (attr) {
        if (p_gcc_jit_function_add_attribute) {
            switch (attr->kind) {
                case ak_always_inline:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_ALWAYS_INLINE);
                    break;
                case ak_cold:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_COLD);
                    break;
                case ak_noinline:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_NOINLINE);
                    break;
                case ak_pure:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_PURE);
                    break;
                case ak_const:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_CONST);
                    break;
                default:
                    break;
            }
        }
        attr = attr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !struct_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    /* Phase 8: Wait for libgccjit APIs exposing struct packing/alignment directly if applicable */
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    if (!var || !lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (p_gcc_jit_lvalue_set_tls_model && var->is_thread_local) {
        /* Phase 8 tls_model */
        p_gcc_jit_lvalue_set_tls_model(lval, GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC);
    }
    
    an_attribute_ptr attr = var->source_corresp.attributes;
    while (attr) {
        if (attr->kind == ak_section && p_gcc_jit_lvalue_set_link_section) {
            an_attribute_arg_ptr arg = attr->arguments;
            if (arg && arg->kind == aak_constant && arg->variant.constant->kind == ck_string) {
                p_gcc_jit_lvalue_set_link_section(lval, (const char*)arg->variant.constant->variant.string.value);
            }
        }
        attr = attr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
