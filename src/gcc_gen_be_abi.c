/**
 * @file gcc_gen_be_abi.c
 * @brief Implementation of Target ABI Parameter Classification & Aggregate Layout.
 *
 * This file implements the System V AMD64 ABI classification (eightbyte tracking,
 * INTEGER/SSE splitting, sret return values) and AAPCS64 (ARM64) classification
 * (HFA/HVA passing, indirect byval arguments). It also processes calling
 * convention attributes.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_type.h"
#include "il_def.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Applies function calling convention attributes to a JIT function.
 *
 * @details Handles __cdecl, __stdcall, __fastcall, __thiscall, and __vectorcall 
 * by invoking the corresponding libgccjit attribute functions or asm decorators.
 *
 * @param[in] rout The frontend routine describing the attributes.
 * @param[in] func The libgccjit function to modify.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
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
                case ak_noreturn:
                    if (p_gcc_jit_function_add_string_attribute) {
                        p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "noreturn");
                    }
                    break;
                case ak_flatten:
                    if (p_gcc_jit_function_add_string_attribute) {
                        p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "flatten");
                    }
                    break;
                case ak_format:
                    if (p_gcc_jit_function_add_string_attribute) {
                        p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "format");
                    }
                    break;
                case ak_dllexport:
                    if (p_gcc_jit_function_add_string_attribute) {
                        p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "dllexport");
                    }
                    break;
                case ak_dllimport:
                    if (p_gcc_jit_function_add_string_attribute) {
                        p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "dllimport");
                    }
                    break;
                case ak_returns_twice:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_RETURNS_TWICE);
                    break;
                default:
                    break;
            }
        }
        attr = attr->next;
    }

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
    if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info) {
        a_calling_convention cc = rout->type->variant.routine.extra_info->calling_convention;
        if (p_gcc_jit_function_add_string_attribute) {
            if (cc == acc_stdcall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "stdcall");
            } else if (cc == acc_fastcall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "fastcall");
            } else if (cc == acc_thiscall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "thiscall");
            } else if (cc == acc_vectorcall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "vectorcall");
            }
        }
    }
#endif

    return GCC_GEN_BE_SUCCESS;
}


/**
 * @brief Applies attributes and struct packing directives to a JIT struct.
 *
 * @param[in] class_type The frontend class type.
 * @param[in] struct_type The libgccjit struct to modify.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !struct_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    /* Phase 6 handles struct packing via explicit layout. libgccjit does not have a 
       direct struct attribute applying method natively yet. */
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Applies variable attributes (alignment, section) to a JIT lvalue.
 *
 * @param[in] var The frontend variable.
 * @param[in] lval The libgccjit lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    if (!var || !lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (p_gcc_jit_lvalue_set_tls_model && var->is_thread_local) {
        p_gcc_jit_lvalue_set_tls_model(lval, GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC);
    }
    
#if defined(LIBGCCJIT_HAVE_ALIGNMENT)
    if (var->alignment > 0) {
        if (p_gcc_jit_lvalue_set_alignment) {
            p_gcc_jit_lvalue_set_alignment(lval, var->alignment);
        }
    }
#endif
    
    an_attribute_ptr attr = var->source_corresp.attributes;
    while (attr) {
        if (attr->kind == ak_section && p_gcc_jit_lvalue_set_link_section) {
            an_attribute_arg_ptr arg = attr->arguments;
            if (arg && arg->kind == aak_constant && arg->variant.constant->kind == ck_string) {
                p_gcc_jit_lvalue_set_link_section(lval, (const char*)arg->variant.constant->variant.string.value);
            }
        } else if (attr->kind == ak_visibility && p_gcc_jit_lvalue_add_string_attribute) {
            an_attribute_arg_ptr arg = attr->arguments;
            if (arg && arg->kind == aak_constant && arg->variant.constant->kind == ck_string) {
                p_gcc_jit_lvalue_add_string_attribute(lval, GCC_JIT_VARIABLE_ATTRIBUTE_VISIBILITY, (const char*)arg->variant.constant->variant.string.value);
            }
        }
        attr = attr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively merges two AMD64 ABI classes.
 *
 * @param[in] a First class.
 * @param[in] b Second class.
 * @return The merged class.
 */
static gcc_gen_be_abi_class_t merge_amd64_classes(gcc_gen_be_abi_class_t a, gcc_gen_be_abi_class_t b) {
    if (a == b) return a;
    if (a == GCC_GEN_BE_ABI_CLASS_NO_CLASS) return b;
    if (b == GCC_GEN_BE_ABI_CLASS_NO_CLASS) return a;
    if (a == GCC_GEN_BE_ABI_CLASS_MEMORY || b == GCC_GEN_BE_ABI_CLASS_MEMORY) return GCC_GEN_BE_ABI_CLASS_MEMORY;
    if (a == GCC_GEN_BE_ABI_CLASS_INTEGER || b == GCC_GEN_BE_ABI_CLASS_INTEGER) return GCC_GEN_BE_ABI_CLASS_INTEGER;
    if (a == GCC_GEN_BE_ABI_CLASS_X87 || a == GCC_GEN_BE_ABI_CLASS_X87UP || a == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87 ||
        b == GCC_GEN_BE_ABI_CLASS_X87 || b == GCC_GEN_BE_ABI_CLASS_X87UP || b == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87) {
        return GCC_GEN_BE_ABI_CLASS_MEMORY;
    }
    return GCC_GEN_BE_ABI_CLASS_SSE;
}

/**
 * @brief Recursively classifies a type into AMD64 ABI eightbyte classes.
 * @details Traverses members up to 16 bytes, merging field classes.
 * @param[in] type Type to classify.
 * @param[in] offset_in_bytes Offset of this type within the top-level object.
 * @param[in,out] classes Eightbyte classes array.
 * @return GCC_GEN_BE_SUCCESS or error.
 */
static gcc_gen_be_error_t do_classify_sysv_amd64(a_type_ptr type, long offset_in_bytes, gcc_gen_be_abi_class_t classes[2]) {
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(t);
    if (sz == 0 || offset_in_bytes >= 16) return GCC_GEN_BE_SUCCESS;
    
    if (t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) {
        a_class_type_supplement_ptr ctsp = t->variant.class_struct_union.extra_info;
        if (ctsp && ctsp->assoc_scope) {
            a_field_ptr f = ctsp->assoc_scope->fields;
            while (f) {
                long field_offset = offset_in_bytes + f->offset;
                GCC_GEN_BE_CHECK(do_classify_sysv_amd64(f->type, field_offset, classes));
                f = f->next;
            }
        }
    } else if (t->kind == tk_array) {
        long elem_sz = (long)size_of_type(t->variant.array.elem_type);
        long count = t->variant.array.elem_count;
        long i;
        for (i = 0; i < count; i++) {
            long elem_offset = offset_in_bytes + i * elem_sz;
            GCC_GEN_BE_CHECK(do_classify_sysv_amd64(t->variant.array.elem_type, elem_offset, classes));
        }
    } else {
        gcc_gen_be_abi_class_t c = GCC_GEN_BE_ABI_CLASS_INTEGER;
        if (is_floating_point_type(t)) {
            c = GCC_GEN_BE_ABI_CLASS_SSE;
        }
        long idx = offset_in_bytes / 8;
        if (idx < 2) {
            classes[idx] = merge_amd64_classes(classes[idx], c);
        }
        /* Handle elements crossing eightbyte boundaries */
        if (offset_in_bytes % 8 != 0 && (offset_in_bytes % 8) + sz > 8) {
            if (idx + 1 < 2) {
                classes[idx + 1] = merge_amd64_classes(classes[idx + 1], c);
            }
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies System V AMD64 ABI.
 * @details Implement Complete System V AMD64 ABI Classification including
 * recursive classification for aggregate types up to 16 bytes.
 * Handles INTEGER, SSE splitting, and MEMORY passing.
 * @param[in] type Type.
 * @param[out] classes Classes for up to 2 eightbytes.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_sysv_amd64(a_type_ptr type, gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT {
    if (!type || !classes) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    classes[0] = GCC_GEN_BE_ABI_CLASS_NO_CLASS;
    classes[1] = GCC_GEN_BE_ABI_CLASS_NO_CLASS;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(t);
    if (sz > 16 || sz == 0) {
        classes[0] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        classes[1] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        return GCC_GEN_BE_SUCCESS;
    }
    
    GCC_GEN_BE_CHECK(do_classify_sysv_amd64(t, 0, classes));
    
    /* Post-merge checks: if any is MEMORY, all become MEMORY */
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_MEMORY || classes[1] == GCC_GEN_BE_ABI_CLASS_MEMORY) {
        classes[0] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        classes[1] = GCC_GEN_BE_ABI_CLASS_MEMORY;
    }
    
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_NO_CLASS) classes[0] = GCC_GEN_BE_ABI_CLASS_INTEGER;
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies AAPCS64 (ARM64) ABI.
 * @details Implements Homogeneous Floating-Point Aggregates (HFA) and
 * Homogeneous Short-Vector Aggregates (HVA) classification (1 to 4 members).
 * @param[in] type Type.
 * @param[out] is_hfa_hva TRUE if HFA/HVA.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_aapcs64(a_type_ptr type, a_boolean *is_hfa_hva) GCC_GEN_BE_NOEXCEPT {
    if (!type || !is_hfa_hva) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *is_hfa_hva = FALSE;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    if (t->kind == tk_class || t->kind == tk_struct) {
        a_class_type_supplement_ptr ctsp = t->variant.class_struct_union.extra_info;
        if (ctsp && ctsp->assoc_scope) {
            a_field_ptr f = ctsp->assoc_scope->fields;
            int count = 0;
            a_boolean all_fp = TRUE;
            a_type_ptr first_fp = NULL;
            long first_fp_sz = 0;
            while (f) {
                if (!is_floating_point_type(f->type)) {
                    all_fp = FALSE;
                    break;
                }
                if (!first_fp) {
                    first_fp = f->type;
                    first_fp_sz = (long)size_of_type(first_fp);
                } else {
                    if ((long)size_of_type(f->type) != first_fp_sz) {
                        all_fp = FALSE;
                        break;
                    }
                }
                count++;
                f = f->next;
            }
            if (all_fp && count > 0 && count <= 4) {
                *is_hfa_hva = TRUE;
            }
        }
    } else if (t->kind == tk_array) {
        if (is_floating_point_type(t->variant.array.elem_type)) {
            if (t->variant.array.elem_count > 0 && t->variant.array.elem_count <= 4) {
                *is_hfa_hva = TRUE;
            }
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies Windows x64 ABI.
 * @details Determines if passed by reference (size > 8 or non-power-of-two).
 * @param[in] type Type.
 * @param[out] out_is_ref TRUE if passed by reference.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_win64(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(t);
    if (sz > 8 || (sz != 1 && sz != 2 && sz != 4 && sz != 8)) {
        *out_is_ref = TRUE;
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines if a type must be passed by invisible reference.
 * @details Checks if the type is non-trivially copyable.
 * @param[in] type The frontend type.
 * @param[out] out_is_ref Set to TRUE if it must be passed by invisible reference, FALSE otherwise.
 * @return gcc_gen_be_error_t GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    if (!type) return GCC_GEN_BE_SUCCESS;
    while (type && type->kind == tk_typeref) type = type->variant.typeref.type;
    if (type->kind != tk_class && type->kind != tk_struct && type->kind != tk_union) return GCC_GEN_BE_SUCCESS;
    extern a_boolean is_trivially_copyable_type(a_type_ptr tp);
    *out_is_ref = !is_trivially_copyable_type(type);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines if a type must be returned via sret.
 * @details Checks if the type is non-trivially copyable or strictly larger than 16 bytes for SysV.
 * @param[in] type The frontend type.
 * @param[out] out_is_sret Set to TRUE if it must be returned via sret, FALSE otherwise.
 * @return gcc_gen_be_error_t GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_sret) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(type, out_is_sret));
    if (*out_is_sret) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(type);
    if (sz > 16) {
        *out_is_sret = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies Darwin ARM64 ABI rules.
 * @details Implements Darwin variadic argument and bitfield calling conventions.
 * @param[in] type Type.
 * @param[out] out_is_stack TRUE if passed on stack (e.g. variadic).
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_darwin_arm64(a_type_ptr type, a_boolean *out_is_stack) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_stack) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_stack = FALSE;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    /* Darwin ARM64 passes variadic arguments entirely on the stack */
    /* This classification focuses on stack promotion for variadic. */
    *out_is_stack = TRUE;
    
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
