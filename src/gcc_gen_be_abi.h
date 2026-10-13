/**
 * @file gcc_gen_be_abi.h
 * @brief ABI, Calling Convention, and Attribute generation for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_ABI_H
#define GCC_GEN_BE_ABI_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Applies ABI attributes to a JIT function.
 *
 * @param rout The front-end routine entity containing attributes.
 * @param func The created libgccjit function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Applies struct packing and alignment attributes.
 *
 * @param class_type The class/struct/union type.
 * @param struct_type The corresponding libgccjit struct.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Applies visibility and linkage attributes to a global variable.
 *
 * @param var The front-end variable entity.
 * @param lval The corresponding libgccjit global lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Represents the System V AMD64 ABI eightbyte classification classes.
 * @details Used to determine register versus memory passing conventions.
 */
typedef enum {
    GCC_GEN_BE_ABI_CLASS_INTEGER,      /**< @brief Passed in general purpose registers. */
    GCC_GEN_BE_ABI_CLASS_SSE,          /**< @brief Passed in SSE/vector registers. */
    GCC_GEN_BE_ABI_CLASS_SSEUP,        /**< @brief Passed in upper halves of SSE registers. */
    GCC_GEN_BE_ABI_CLASS_X87,          /**< @brief Passed in x87 FPU registers. */
    GCC_GEN_BE_ABI_CLASS_X87UP,        /**< @brief Passed in upper halves of x87 FPU registers. */
    GCC_GEN_BE_ABI_CLASS_COMPLEX_X87,  /**< @brief Passed as complex x87 values. */
    GCC_GEN_BE_ABI_CLASS_NO_CLASS,     /**< @brief Ignored or empty class. */
    GCC_GEN_BE_ABI_CLASS_MEMORY        /**< @brief Passed in memory (stack). */
} gcc_gen_be_abi_class_t;

/**
 * @brief Classifies a type according to System V AMD64 ABI.
 * @details Implements the recursive eightbyte classification algorithm.
 *          Aggregate types up to 16 bytes are classified into up to 2 eightbytes.
 * @param[in] type The frontend type to classify.
 * @param[out] classes An array of 2 classes receiving the eightbyte classifications.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_sysv_amd64(a_type_ptr type, gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to AAPCS64 (ARM64) ABI.
 * @details Determines if a type is a Homogeneous Floating-Point Aggregate (HFA)
 *          or Homogeneous Short-Vector Aggregate (HVA).
 * @param[in] type The frontend type to classify.
 * @param[out] is_hfa_hva Set to TRUE if the type is HFA/HVA, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_aapcs64(a_type_ptr type, a_boolean *is_hfa_hva) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to Windows x64 ABI.
 * @details Determines if a type must be passed by reference (e.g. non-power-of-two size or > 8 bytes).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_ref Set to TRUE if the type must be passed by reference, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_win64(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Determines if a type must be passed by invisible reference.
 * @details Checks if the type is non-trivially copyable.
 * @param[in] type The frontend type.
 * @param[out] out_is_ref Set to TRUE if it must be passed by invisible reference, FALSE otherwise.
 * @return gcc_gen_be_error_t GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
/**
 * @brief Determines if a type must be returned via sret.
 * @details Checks if the type is non-trivially copyable.
 * @param[in] type The frontend type.
 * @return TRUE if it must be returned via sret, FALSE otherwise.
 */
extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies Darwin ARM64 ABI rules.
 * @details Implements Darwin variadic argument and bitfield calling conventions.
 * @param[in] type Type.
 * @param[out] out_is_stack TRUE if passed on stack (e.g. variadic).
 * @return GCC_GEN_BE_SUCCESS
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_darwin_arm64(a_type_ptr type, a_boolean *out_is_stack) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_ABI_H */

