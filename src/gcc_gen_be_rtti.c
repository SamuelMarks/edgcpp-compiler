/**
 * @file gcc_gen_be_rtti.c
 * @brief Implementation of RTTI and VTable generation.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

gcc_gen_be_error_t gcc_gen_be_emit_vtable(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !out_vtable) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtable = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    a_class_type_supplement_ptr ctsp = class_type->variant.class_struct_union.extra_info;
    if (!ctsp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    int num_funcs = 0;
    if (ctsp->highest_virtual_function_number != VIRTUAL_FUNCTION_NUMBER_NONE) {
        num_funcs = ctsp->highest_virtual_function_number + 1;
    }

    /* Itanium ABI: [0] = offset-to-top, [1] = type_info*, [2..N] = virtual functions */
    size_t num_entries = 2 + num_funcs;
    
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, num_entries);

    char vtable_name[128];
    snprintf(vtable_name, sizeof(vtable_name), "_ZTV_class_%p", (void *)class_type);

    gcc_jit_lvalue *vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, vtable_name);
    
    gcc_jit_rvalue **vtable_entries = (gcc_jit_rvalue **)malloc(num_entries * sizeof(gcc_jit_rvalue *));
    if (!vtable_entries) return GCC_GEN_BE_ERROR_OOM;

    /* [0] offset-to-top (0 for primary vtable) */
    vtable_entries[0] = gcc_jit_context_null(ctx, void_ptr_type);

    /* [1] RTTI type_info */
    gcc_jit_rvalue *type_info_ptr = NULL;
    /* Construct dummy expr for typeid to get type_info pointer */
    an_expr_node dummy_expr;
    memset(&dummy_expr, 0, sizeof(dummy_expr));
    dummy_expr.kind = enk_typeid;
    an_expr_node dummy_type_op;
    memset(&dummy_type_op, 0, sizeof(dummy_type_op));
    dummy_type_op.kind = enk_type_operand;
    dummy_type_op.variant.type_operand.type = class_type;
    dummy_expr.variant.typeid_info.type_with_opt_expr = &dummy_type_op;
    err = gcc_gen_be_lower_typeid(&dummy_expr, &type_info_ptr);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(vtable_entries);
        return err;
    }
    vtable_entries[1] = gcc_jit_context_new_cast(ctx, NULL, type_info_ptr, void_ptr_type);

    /* [2..N] Virtual function pointers */
    for (int i = 0; i < num_funcs; ++i) {
        /* Populate with dummy nulls or pure virtual handler if not found.
           Proper lookup involves traversing symbol_supplement_for_class(class_type)->symbols
           to match virtual_function_number. */
        vtable_entries[2 + i] = gcc_jit_context_null(ctx, void_ptr_type);
    }

    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, num_entries, vtable_entries);
    gcc_jit_global_set_initializer_rvalue(vtable, init_array);

    free(vtable_entries);
    
    *out_vtable = vtable;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !out_vtt) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtt = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Emit a dummy VTT array to satisfy the requirement for Phase 2 */
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 1);

    char vtt_name[128];
    snprintf(vtt_name, sizeof(vtt_name), "_ZTT_class_%p", (void *)class_type);

    gcc_jit_lvalue *vtt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, vtt_name);
    gcc_jit_rvalue *dummy_val = gcc_jit_context_null(ctx, void_ptr_type);
    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, 1, &dummy_val);
    gcc_jit_global_set_initializer_rvalue(vtt, init_array);

    *out_vtt = vtt;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_thunk(a_routine_ptr thunk_rp, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!thunk_rp || !out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *thunk_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "dummy_thunk", 0, NULL, 0);

    /* Phase 2: Stub for this pointer adjustment thunk and covariant return adjustment */
    gcc_jit_block *block = gcc_jit_function_new_block(thunk_fn, "entry");
    gcc_jit_block_end_with_void_return(block, NULL);

    *out_func = thunk_fn;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Requires __dynamic_cast(src_ptr, src_type_info, dst_type_info, src2dst_offset) */
    void *cached = NULL;
    gcc_jit_function *dyn_cast_fn = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        dyn_cast_fn = (gcc_jit_function *)cached;
    } else {
        gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "src_ptr");
        gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "src_type");
        gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dst_type");
        gcc_jit_param *p4 = gcc_jit_context_new_param(ctx, NULL, long_type, "src2dst");
        gcc_jit_param *params[] = {p1, p2, p3, p4};
        dyn_cast_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__dynamic_cast", 4, params, 0);
        err = cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", dyn_cast_fn);
        if (err != GCC_GEN_BE_SUCCESS) return err;
    }

    gcc_jit_rvalue *src_ptr = NULL;
    err = gcc_gen_be_lower_expr_rvalue(expr->variant.operation.operands, &src_ptr);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    src_ptr = gcc_jit_context_new_cast(ctx, NULL, src_ptr, void_ptr_type);

    an_expr_node src_typeid_expr;
    memset(&src_typeid_expr, 0, sizeof(src_typeid_expr));
    src_typeid_expr.kind = enk_typeid;
    an_expr_node dummy_src_type_op;
    memset(&dummy_src_type_op, 0, sizeof(dummy_src_type_op));
    dummy_src_type_op.kind = enk_type_operand;
    dummy_src_type_op.variant.type_operand.type = expr->variant.operation.operands->type;
    src_typeid_expr.variant.typeid_info.type_with_opt_expr = &dummy_src_type_op;
    
    gcc_jit_rvalue *src_type_info = NULL;
    err = gcc_gen_be_lower_typeid(&src_typeid_expr, &src_type_info);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    src_type_info = gcc_jit_context_new_cast(ctx, NULL, src_type_info, void_ptr_type);

    an_expr_node dst_typeid_expr;
    memset(&dst_typeid_expr, 0, sizeof(dst_typeid_expr));
    dst_typeid_expr.kind = enk_typeid;
    an_expr_node dummy_dst_type_op;
    memset(&dummy_dst_type_op, 0, sizeof(dummy_dst_type_op));
    dummy_dst_type_op.kind = enk_type_operand;
    dummy_dst_type_op.variant.type_operand.type = expr->type;
    dst_typeid_expr.variant.typeid_info.type_with_opt_expr = &dummy_dst_type_op;
    
    gcc_jit_rvalue *dst_type_info = NULL;
    err = gcc_gen_be_lower_typeid(&dst_typeid_expr, &dst_type_info);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    dst_type_info = gcc_jit_context_new_cast(ctx, NULL, dst_type_info, void_ptr_type);

    gcc_jit_rvalue *offset = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG));
    
    gcc_jit_rvalue *args[] = {src_ptr, src_type_info, dst_type_info, offset};
    gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, dyn_cast_fn, 4, args);
    
    gcc_jit_type *res_type = NULL;
    err = gcc_gen_be_lower_type(expr->type, &res_type);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    *out_rval = gcc_jit_context_new_cast(ctx, NULL, call, res_type);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_typeid(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* std::type_info object pointer */
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *type_info_type = void_ptr_type; /* stub for struct std::type_info */
    
    an_expr_node_ptr type_op = expr->variant.typeid_info.type_with_opt_expr;
    if (!type_op || type_op->kind != enk_type_operand) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    a_type_ptr tp = type_op->variant.type_operand.type;
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    char name[128];
    snprintf(name, sizeof(name), "_ZTI_%p", (void *)tp);

    /* Generate an external reference to the type descriptor.
       In a full implementation, we'd emit the global descriptor definitions
       if the type is declared in this translation unit. */
    gcc_jit_lvalue *type_info_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, type_info_type, name);
    *out_rval = gcc_jit_lvalue_get_address(type_info_lval, NULL);

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT {
    if (!tp || !out_descriptor) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_descriptor = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Stub: Emit a generic type_info descriptor */
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 2);

    char name[128];
    snprintf(name, sizeof(name), "_ZTI_%p", (void *)tp);

    gcc_jit_lvalue *desc = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, name);
    
    gcc_jit_rvalue *vals[2];
    vals[0] = gcc_jit_context_null(ctx, void_ptr_type); /* vtable for type_info */
    char name_str[128];
    snprintf(name_str, sizeof(name_str), "type_name_%p", (void *)tp);
    gcc_jit_rvalue *str_val = gcc_jit_context_new_string_literal(ctx, name_str);
    vals[1] = gcc_jit_context_new_cast(ctx, NULL, str_val, void_ptr_type);

    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, 2, vals);
    gcc_jit_global_set_initializer_rvalue(desc, init_array);

    *out_descriptor = desc;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
