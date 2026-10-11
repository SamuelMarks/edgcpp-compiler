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

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_ABI_H */
