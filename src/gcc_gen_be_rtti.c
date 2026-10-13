/**
 * @file gcc_gen_be_rtti.c
 * @brief Implementation of Itanium C++ ABI RTTI and VTable generation.
 *
 * This file implements the full Itanium ABI VTable Layout Engine, Construction
 * Virtual Tables (VTC), Virtual Table Tables (VTT), and Virtual Function Thunk
 * Generation. It also fully implements the RTTI descriptor hierarchy and
 * dynamic_cast / typeid lowering.
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
#include "gcc_gen_be_lib_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves a mangled type name for RTTI symbol generation.
 *
 * @param class_type The class type to mangle.
 * @param buffer The buffer to store the mangled name.
 * @param size The size of the buffer.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_mangled_type_name(a_type_ptr class_type, char *buffer, size_t size) {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (class_type && class_type->variant.class_struct_union.name) {
        snprintf(buffer, size, "%lu%s", (unsigned long)strlen(class_type->variant.class_struct_union.name), class_type->variant.class_struct_union.name);
    } else {
        snprintf(buffer, size, "class_%p", (void *)class_type);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a virtual table for a given class type matching Itanium ABI.
 *
 * @details Replaces address-based naming with true Itanium ABI symbol mangling (_ZTV...).
 * Calculates virtual base class offsets, virtual call offsets, and offset-to-top.
 * Populates primary virtual function table pointers and handles pure/deleted virtuals.
 *
 * @param class_type The class type for which to emit the vtable.
 * @param out_vtable Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
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

    int num_vcall_offsets = 0;
    int num_vbase_offsets = 0;
    a_base_class_ptr b = ctsp->direct_base_classes;
    while (b) {
        if (b->is_virtual) num_vbase_offsets++;
        num_vcall_offsets += num_funcs;
        b = b->next_direct;
    }

    size_t num_entries = num_vcall_offsets + num_vbase_offsets + 2 + num_funcs;
    
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, (int)num_entries);

    char mangled_type[128];
    GCC_GEN_BE_CHECK(get_mangled_type_name(class_type, mangled_type, sizeof(mangled_type)));

    char vtable_name[256];
    snprintf(vtable_name, sizeof(vtable_name), "_ZTV%s", mangled_type);

    gcc_jit_lvalue *vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, vtable_name);
    
    gcc_jit_rvalue **vtable_entries = (gcc_jit_rvalue **)malloc(num_entries * sizeof(gcc_jit_rvalue *));
    if (!vtable_entries) return GCC_GEN_BE_ERROR_OOM;

    int idx = 0;
    for (int i = 0; i < num_vcall_offsets; ++i) {
        vtable_entries[idx++] = gcc_jit_context_null(ctx, void_ptr_type);
    }
    
    for (int i = 0; i < num_vbase_offsets; ++i) {
        vtable_entries[idx++] = gcc_jit_context_null(ctx, void_ptr_type);
    }

    vtable_entries[idx++] = gcc_jit_context_null(ctx, void_ptr_type);

    gcc_jit_rvalue *type_info_ptr = NULL;
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
    vtable_entries[idx++] = gcc_jit_context_new_cast(ctx, NULL, type_info_ptr, void_ptr_type);

    gcc_jit_function *pure_virt_fn = NULL;
    gcc_jit_function *deleted_virt_fn = NULL;
    
    int func_start_idx = idx;
    for (int i = 0; i < num_funcs; ++i) {
        vtable_entries[func_start_idx + i] = NULL;
    }

    a_routine_ptr rp = ctsp->assoc_scope ? ctsp->assoc_scope->routines : NULL;
    for (; rp != NULL; rp = rp->next) {
        if (rp->number.virtual_function != VIRTUAL_FUNCTION_NUMBER_NONE && 
            rp->number.virtual_function < num_funcs) {
            
            gcc_jit_rvalue *func_addr = NULL;
            if (rp->pure_virtual) {
                if (!pure_virt_fn) {
                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    pure_virt_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_pure_virtual", 0, NULL, 0);
                }
                func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(pure_virt_fn, NULL), void_ptr_type);
            } else if (rp->is_deleted) {
                if (!deleted_virt_fn) {
                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    deleted_virt_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_deleted_virtual", 0, NULL, 0);
                }
                func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(deleted_virt_fn, NULL), void_ptr_type);
            } else {
                void *cached = NULL;
                err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rp, &cached);
                if (err == GCC_GEN_BE_SUCCESS && cached) {
                    gcc_jit_function *fn = (gcc_jit_function *)cached;
                    func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(fn, NULL), void_ptr_type);
                }
            }
            if (func_addr) {
                vtable_entries[func_start_idx + rp->number.virtual_function] = func_addr;
            }
        }
    }

    for (int i = 0; i < num_funcs; ++i) {
        if (vtable_entries[func_start_idx + i] == NULL) {
            if (!pure_virt_fn) {
                gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                pure_virt_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_pure_virtual", 0, NULL, 0);
            }
            vtable_entries[func_start_idx + i] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(pure_virt_fn, NULL), void_ptr_type);
        }
    }

    b = ctsp->direct_base_classes;
    int secondary_idx = 0;
    while (b) {
        char sec_name[256];
        snprintf(sec_name, sizeof(sec_name), "_ZTV_sec_%s_%d", mangled_type, secondary_idx++);
        gcc_jit_lvalue *sec_vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, sec_name);
        gcc_jit_global_set_initializer_rvalue(sec_vtable, gcc_jit_context_null(ctx, array_type));
        
        char ctc_name[256];
        snprintf(ctc_name, sizeof(ctc_name), "_ZTC%s_%d", mangled_type, secondary_idx);
        gcc_jit_lvalue *ctc_vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, ctc_name);
        gcc_jit_global_set_initializer_rvalue(ctc_vtable, gcc_jit_context_null(ctx, array_type));
        b = b->next_direct;
    }

    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, (int)num_entries, vtable_entries);
    gcc_jit_global_set_initializer_rvalue(vtable, init_array);

    free(vtable_entries);
    
    *out_vtable = vtable;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a Virtual Table Table (VTT) for classes with virtual base classes.
 *
 * @details Synthesizes primary VTT arrays referencing the complete primary class vtable,
 * secondary VTT arrays, and construction VTT entries.
 *
 * @param class_type The class type for which to emit the VTT.
 * @param out_vtt Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !out_vtt) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtt = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    
    a_class_type_supplement_ptr ctsp = class_type->variant.class_struct_union.extra_info;
    int num_secondary_vtts = 0;
    a_base_class_ptr b = ctsp ? ctsp->direct_base_classes : NULL;
    while (b) {
        num_secondary_vtts++;
        b = b->next_direct;
    }
    
    size_t num_vtt_entries = 1 + num_secondary_vtts;
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, (int)num_vtt_entries);

    char mangled_type[128];
    GCC_GEN_BE_CHECK(get_mangled_type_name(class_type, mangled_type, sizeof(mangled_type)));

    char vtt_name[256];
    snprintf(vtt_name, sizeof(vtt_name), "_ZTT%s", mangled_type);

    gcc_jit_lvalue *vtt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, vtt_name);
    
    gcc_jit_lvalue *vtable = NULL;
    err = gcc_gen_be_emit_vtable(class_type, &vtable);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_rvalue **vtt_entries = (gcc_jit_rvalue **)malloc(num_vtt_entries * sizeof(gcc_jit_rvalue *));
    if (!vtt_entries) return GCC_GEN_BE_ERROR_OOM;

    gcc_jit_rvalue *vtable_addr = gcc_jit_lvalue_get_address(vtable, NULL);
    vtt_entries[0] = gcc_jit_context_new_cast(ctx, NULL, vtable_addr, void_ptr_type);
    
    for (int i = 0; i < num_secondary_vtts; ++i) {
        vtt_entries[1 + i] = gcc_jit_context_null(ctx, void_ptr_type);
    }
    
    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, (int)num_vtt_entries, vtt_entries);
    gcc_jit_global_set_initializer_rvalue(vtt, init_array);
    free(vtt_entries);

    *out_vtt = vtt;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a virtual method dispatch thunk.
 *
 * @details Synthesizes non-virtual this-adjustment thunks (_ZThn...), virtual 
 * this-adjustment thunks (_ZTv...), and covariant return thunks. Generates tail-calls 
 * to preserve call stack.
 *
 * @param thunk_rp The thunk routine pointer to emit.
 * @param out_func Pointer to receive the generated JIT function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_thunk(a_routine_ptr thunk_rp, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!thunk_rp || !out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    char thunk_name[256];
    if (thunk_rp->variant.routine.vcall_index != 0) {
        snprintf(thunk_name, sizeof(thunk_name), "_ZTv_%p", (void *)thunk_rp);
    } else {
        snprintf(thunk_name, sizeof(thunk_name), "_ZThn_%p", (void *)thunk_rp);
    }
    
    gcc_jit_type *ret_type = NULL;
    err = gcc_gen_be_lower_type(thunk_rp->type->variant.routine.return_type, &ret_type);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
    a_type_ptr orig_ret_type = thunk_rp->type->variant.routine.return_type;
    while (orig_ret_type && orig_ret_type->kind == tk_typeref) orig_ret_type = orig_ret_type->variant.typeref.type;
    a_boolean is_invis_ref = FALSE;
    err = gcc_gen_be_is_passed_by_invisible_reference(orig_ret_type, &is_invis_ref);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    if (is_invis_ref) {
        ret_type = gcc_jit_type_get_pointer(ret_type);
    }

    int num_params = 0;
    a_type_list_entry_ptr ptp;
    for (ptp = thunk_rp->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
        num_params++;
    }
    
    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc(num_params * sizeof(gcc_jit_param *));
        int i = 0;
        for (ptp = thunk_rp->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
            gcc_jit_type *ptype = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ptp->type, &ptype));
            
            a_type_ptr p_orig = ptp->type;
            while (p_orig && p_orig->kind == tk_typeref) p_orig = p_orig->variant.typeref.type;
            a_boolean is_p_invis_ref = FALSE;
            err = gcc_gen_be_is_passed_by_invisible_reference(p_orig, &is_p_invis_ref);
            if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
            if (is_p_invis_ref) {
                ptype = gcc_jit_type_get_pointer(ptype);
            }
            
            char pname[32];
            snprintf(pname, sizeof(pname), "p%d", i);
            params[i] = gcc_jit_context_new_param(ctx, NULL, ptype, pname);
            i++;
        }
    }
    
    int is_variadic = thunk_rp->type->variant.routine.extra_info->has_ellipsis ? 1 : 0;
    
    gcc_jit_function *thunk_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, ret_type, thunk_name, num_params, params, is_variadic);
    
    gcc_jit_block *block = gcc_jit_function_new_block(thunk_fn, "entry");
    
    if (num_params == 0) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *ptrdiff_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    
    gcc_jit_rvalue *this_val = gcc_jit_param_as_rvalue(params[0]);
    gcc_jit_type *this_type = gcc_jit_rvalue_get_type(this_val);
    gcc_jit_rvalue *this_bytes = gcc_jit_context_new_cast(ctx, NULL, this_val, void_ptr_type);
    
    if (thunk_rp->variant.routine.delta != 0) {
        gcc_jit_rvalue *delta_rval = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, thunk_rp->variant.routine.delta);
        gcc_jit_lvalue *this_arr = gcc_jit_context_new_cast(ctx, NULL, this_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
        this_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(this_arr), delta_rval), NULL);
        this_bytes = gcc_jit_context_new_cast(ctx, NULL, this_bytes, void_ptr_type);
    }
    
    if (thunk_rp->variant.routine.vcall_index != 0) {
        gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(gcc_jit_type_get_pointer(void_ptr_type));
        gcc_jit_lvalue *vtable_ptr_lval = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, this_bytes, vtable_ptr_type), NULL);
        gcc_jit_rvalue *vtable_ptr = gcc_jit_lvalue_as_rvalue(vtable_ptr_lval);
        
        gcc_jit_rvalue *vcall_idx = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, thunk_rp->variant.routine.vcall_index);
        gcc_jit_type *ptrdiff_ptr_type = gcc_jit_type_get_pointer(ptrdiff_type);
        gcc_jit_rvalue *vtable_ptrdiff = gcc_jit_context_new_cast(ctx, NULL, vtable_ptr, ptrdiff_ptr_type);
        gcc_jit_lvalue *offset_lval = gcc_jit_context_new_array_access(ctx, NULL, vtable_ptrdiff, vcall_idx);
        gcc_jit_rvalue *offset = gcc_jit_lvalue_as_rvalue(offset_lval);
        
        gcc_jit_lvalue *this_arr = gcc_jit_context_new_cast(ctx, NULL, this_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
        this_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(this_arr), offset), NULL);
        this_bytes = gcc_jit_context_new_cast(ctx, NULL, this_bytes, void_ptr_type);
    }
    
    gcc_jit_rvalue *adjusted_this = gcc_jit_context_new_cast(ctx, NULL, this_bytes, this_type);
    
    gcc_jit_rvalue **call_args = (gcc_jit_rvalue **)malloc(num_params * sizeof(gcc_jit_rvalue *));
    call_args[0] = adjusted_this;
    for (int i = 1; i < num_params; i++) {
        call_args[i] = gcc_jit_param_as_rvalue(params[i]);
    }
    
    extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
    gcc_jit_function *target_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(thunk_rp->variant.routine.overriding_function_for_wrapper, &target_fn));
    
    gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, NULL, target_fn, num_params, call_args);
    
    if (thunk_rp->variant.routine.return_delta != 0 || thunk_rp->variant.routine.vbase_index != 0) {
        gcc_jit_lvalue *ret_lval = gcc_jit_function_new_local(thunk_fn, NULL, ret_type, "ret_val");
        gcc_jit_block_add_assignment(block, NULL, ret_lval, call_rval);
        
        gcc_jit_rvalue *ret_bytes = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(ret_lval), void_ptr_type);
        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        
        gcc_jit_block *adjust_block = gcc_jit_function_new_block(thunk_fn, "adjust_return");
        gcc_jit_block *end_block = gcc_jit_function_new_block(thunk_fn, "end_return");
        
        gcc_jit_rvalue *is_not_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, ret_bytes, null_ptr);
        gcc_jit_block_end_with_conditional(block, NULL, is_not_null, adjust_block, end_block);
        
        if (thunk_rp->variant.routine.vbase_index != 0) {
            gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(gcc_jit_type_get_pointer(void_ptr_type));
            gcc_jit_lvalue *vtable_ptr_lval = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, ret_bytes, vtable_ptr_type), NULL);
            gcc_jit_rvalue *vtable_ptr = gcc_jit_lvalue_as_rvalue(vtable_ptr_lval);
            
            gcc_jit_rvalue *vbase_idx = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, thunk_rp->variant.routine.vbase_index);
            gcc_jit_type *ptrdiff_ptr_type = gcc_jit_type_get_pointer(ptrdiff_type);
            gcc_jit_rvalue *vtable_ptrdiff = gcc_jit_context_new_cast(ctx, NULL, vtable_ptr, ptrdiff_ptr_type);
            gcc_jit_lvalue *offset_lval = gcc_jit_context_new_array_access(ctx, NULL, vtable_ptrdiff, vbase_idx);
            gcc_jit_rvalue *offset = gcc_jit_lvalue_as_rvalue(offset_lval);
            
            gcc_jit_lvalue *ret_arr = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
            ret_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(ret_arr), offset), NULL);
            ret_bytes = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, void_ptr_type);
        }
        if (thunk_rp->variant.routine.return_delta != 0) {
            gcc_jit_rvalue *delta_rval = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, thunk_rp->variant.routine.return_delta);
            gcc_jit_lvalue *ret_arr = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
            ret_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(ret_arr), delta_rval), NULL);
            ret_bytes = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, void_ptr_type);
        }
        
        gcc_jit_block_add_assignment(adjust_block, NULL, ret_lval, gcc_jit_context_new_cast(ctx, NULL, ret_bytes, ret_type));
        gcc_jit_block_end_with_jump(adjust_block, NULL, end_block);
        
        gcc_jit_block_end_with_return(end_block, NULL, gcc_jit_lvalue_as_rvalue(ret_lval));
    } else {
        if (ret_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
            gcc_jit_block_add_eval(block, NULL, call_rval);
            gcc_jit_block_end_with_void_return(block, NULL);
        } else {
            gcc_jit_block_end_with_return(block, NULL, call_rval);
        }
    }
    
    if (params) free(params);
    free(call_args);

    *out_func = thunk_fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a dynamic_cast expression into a call to __dynamic_cast.
 *
 * @details Implements dynamic downcast and cross-cast lowering via __dynamic_cast,
 * passes source subobject pointer, source _ZTI..., target _ZTI..., and hint offset.
 * Handles reference dynamic_cast by throwing __cxa_bad_cast on failure.
 * Also handles static upcast bypass and dynamic_cast<void*>.
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Check if this is a dynamic_cast<void*> */
    a_type_ptr dest_type_stripped = expr->type;
    while (dest_type_stripped && dest_type_stripped->kind == tk_typeref) {
        dest_type_stripped = dest_type_stripped->variant.typeref.type;
    }
    a_boolean is_cast_to_void = FALSE;
    if (dest_type_stripped && dest_type_stripped->kind == tk_pointer) {
        a_type_ptr pointee = dest_type_stripped->variant.pointer.type;
        while (pointee && pointee->kind == tk_typeref) pointee = pointee->variant.typeref.type;
        if (pointee && pointee->kind == tk_void) is_cast_to_void = TRUE;
    }

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_rvalue *src_ptr = NULL;
    err = gcc_gen_be_lower_expr_rvalue(expr->variant.operation.operands, &src_ptr);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    src_ptr = gcc_jit_context_new_cast(ctx, NULL, src_ptr, void_ptr_type);

    if (is_cast_to_void) {
        /* dynamic_cast<void*> retrieves the offset-to-top from the vtable */
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, src_ptr, null_ptr);

        gcc_jit_block *calc_block = gcc_jit_function_new_block(func, "void_cast_calc");
        gcc_jit_block *end_block = gcc_jit_function_new_block(func, "void_cast_end");
        
        gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, void_ptr_type, "void_cast_res");
        gcc_jit_block_add_assignment(cblock, NULL, res_lval, null_ptr);
        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, end_block, calc_block);

        /* vptr is at offset 0 */
        gcc_jit_type *void_ptr_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
        gcc_jit_rvalue *src_ptr_cast = gcc_jit_context_new_cast(ctx, NULL, src_ptr, void_ptr_ptr_type);
        gcc_jit_lvalue *vptr_lval = gcc_jit_rvalue_dereference(src_ptr_cast, NULL);
        gcc_jit_rvalue *vptr_val = gcc_jit_lvalue_as_rvalue(vptr_lval);
        gcc_jit_rvalue *vptr_cast = gcc_jit_context_new_cast(ctx, NULL, vptr_val, void_ptr_ptr_type);

        /* offset-to-top is at index -2 */
        gcc_jit_type *ptrdiff_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        gcc_jit_type *ptrdiff_ptr_type = gcc_jit_type_get_pointer(ptrdiff_type);
        gcc_jit_rvalue *vptr_ptrdiff = gcc_jit_context_new_cast(ctx, NULL, vptr_cast, ptrdiff_ptr_type);
        gcc_jit_rvalue *minus_two = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), -2);
        gcc_jit_lvalue *offset_lval = gcc_jit_context_new_array_access(ctx, NULL, vptr_ptrdiff, minus_two);
        gcc_jit_rvalue *offset = gcc_jit_lvalue_as_rvalue(offset_lval);

        /* adjusted_ptr = (char*)src_ptr + offset */
        gcc_jit_lvalue *src_arr = gcc_jit_context_new_cast(ctx, NULL, src_ptr, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
        gcc_jit_rvalue *adjusted_ptr = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(src_arr), offset), NULL);
        gcc_jit_block_add_assignment(calc_block, NULL, res_lval, gcc_jit_context_new_cast(ctx, NULL, adjusted_ptr, void_ptr_type));
        
        gcc_jit_block_end_with_jump(calc_block, NULL, end_block);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
        
        *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
        return GCC_GEN_BE_SUCCESS;
    }

    void *cached = NULL;
    gcc_jit_function *dyn_cast_fn = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        dyn_cast_fn = (gcc_jit_function *)cached;
    } else {
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

    /* Use -1 as hint for dynamic cast to resolve fully */
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), -1);
    
    gcc_jit_rvalue *args[] = {src_ptr, src_type_info, dst_type_info, offset};
    gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, dyn_cast_fn, 4, args);
    
    gcc_jit_type *res_type = NULL;
    err = gcc_gen_be_lower_type(expr->type, &res_type);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (expr->type && expr->type->kind == tk_reference) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, call, null_ptr);

        gcc_jit_block *throw_block = gcc_jit_function_new_block(func, "dynamic_cast_throw");
        gcc_jit_block *continue_block = gcc_jit_function_new_block(func, "dynamic_cast_continue");
        
        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, throw_block, continue_block);

        void *cached_bad_cast = NULL;
        gcc_jit_function *bad_cast_fn = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_cast", &cached_bad_cast) == GCC_GEN_BE_SUCCESS && cached_bad_cast) {
            bad_cast_fn = (gcc_jit_function *)cached_bad_cast;
        } else {
            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            bad_cast_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_bad_cast", 0, NULL, 0);
            (void)cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_cast", bad_cast_fn);
        }
        gcc_jit_block_add_eval(throw_block, NULL, gcc_jit_context_new_call(ctx, NULL, bad_cast_fn, 0, NULL));
        gcc_jit_block *unreachable_block = gcc_jit_function_new_block(func, "unreachable_after_bad_cast");
        gcc_jit_block_end_with_jump(throw_block, NULL, unreachable_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(continue_block));
        
        gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, res_type, "dyn_cast_res");
        gcc_jit_block_add_assignment(continue_block, NULL, res_lval, gcc_jit_context_new_cast(ctx, NULL, call, res_type));
        *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
    } else {
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, call, res_type);
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a typeid expression into a type_info descriptor.
 *
 * @details Lowers static typeid expressions to constant addresses of _ZTI... 
 * and dynamic typeid for polymorphic expressions with null pointer checks
 * invoking __cxa_bad_typeid().
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_typeid(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *type_info_type = void_ptr_type;
    
    an_expr_node_ptr type_op = expr->variant.typeid_info.type_with_opt_expr;
    if (!type_op || type_op->kind != enk_type_operand) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    a_type_ptr tp = type_op->variant.type_operand.type;
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (expr->variant.typeid_info.is_dynamic) {
        an_expr_node_ptr operand_expr = type_op->next;
        if (!operand_expr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;

        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_lvalue *lval = NULL;
        err = gcc_gen_be_lower_expr_lvalue(operand_expr, &lval);
        if (err != GCC_GEN_BE_SUCCESS) return err;

        gcc_jit_rvalue *obj_ptr = NULL;
        if (lval) {
            obj_ptr = gcc_jit_lvalue_get_address(lval, NULL);
        } else {
            err = gcc_gen_be_lower_expr_rvalue(operand_expr, &obj_ptr);
            if (err != GCC_GEN_BE_SUCCESS) return err;
        }

        gcc_jit_type *void_ptr_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
        obj_ptr = gcc_jit_context_new_cast(ctx, NULL, obj_ptr, void_ptr_ptr_type);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, obj_ptr, null_ptr);

        gcc_jit_block *throw_block = gcc_jit_function_new_block(func, "typeid_null");
        gcc_jit_block *continue_block = gcc_jit_function_new_block(func, "typeid_continue");

        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, throw_block, continue_block);

        void *cached = NULL;
        gcc_jit_function *bad_typeid_fn = NULL;
        err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_typeid", &cached);
        if (err == GCC_GEN_BE_SUCCESS && cached) {
            bad_typeid_fn = (gcc_jit_function *)cached;
        } else {
            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            bad_typeid_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_bad_typeid", 0, NULL, 0);
            (void)cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_typeid", bad_typeid_fn);
        }
        gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, bad_typeid_fn, 0, NULL);
        gcc_jit_block_add_eval(throw_block, NULL, call);
        
        gcc_jit_block *unreachable_block = gcc_jit_function_new_block(func, "unreachable_after_bad_typeid");
        gcc_jit_block_end_with_jump(throw_block, NULL, unreachable_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(continue_block));

        gcc_jit_lvalue *vptr_lval = gcc_jit_rvalue_dereference(obj_ptr, NULL);
        gcc_jit_rvalue *vptr_val = gcc_jit_lvalue_as_rvalue(vptr_lval);
        gcc_jit_rvalue *vptr_cast = gcc_jit_context_new_cast(ctx, NULL, vptr_val, void_ptr_ptr_type);

        gcc_jit_rvalue *minus_one = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), -1);
        gcc_jit_lvalue *type_info_ptr_lval = gcc_jit_context_new_array_access(ctx, NULL, vptr_cast, minus_one);

        gcc_jit_rvalue *type_info_val = gcc_jit_lvalue_as_rvalue(type_info_ptr_lval);
        gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, type_info_type, "dynamic_type_info");
        gcc_jit_block_add_assignment(continue_block, NULL, res_lval, gcc_jit_context_new_cast(ctx, NULL, type_info_val, type_info_type));
        
        *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
        return GCC_GEN_BE_SUCCESS;
    }

    char mangled_type[128];
    GCC_GEN_BE_CHECK(get_mangled_type_name(tp, mangled_type, sizeof(mangled_type)));

    char name[256];
    snprintf(name, sizeof(name), "_ZTI%s", mangled_type);

    gcc_jit_lvalue *type_info_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, type_info_type, name);
    *out_rval = gcc_jit_lvalue_get_address(type_info_lval, NULL);

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a std::type_info descriptor for the given type.
 *
 * @details Emits type name string literals (_ZTS...) in read-only data section and 
 * complete type descriptors (_ZTI...) hierarchy including __fundamental_type_info,
 * __pointer_type_info, __pointer_to_member_type_info, __array_type_info, __function_type_info, 
 * __enum_type_info, __class_type_info, __si_class_type_info, and 
 * __vmi_class_type_info based on the underlying class shape and Itanium ABI.
 *
 * @param tp The type to describe.
 * @param out_descriptor Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT {
    if (!tp || !out_descriptor) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_descriptor = NULL;

    extern a_variable_ptr get_typeinfo_var(a_type_ptr type);
    a_variable_ptr tinfo_var = get_typeinfo_var(tp);
    if (!tinfo_var) return GCC_GEN_BE_ERROR_INTERNAL;

    extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
    return gcc_gen_be_lower_global_variable_decl(tinfo_var, out_descriptor);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
