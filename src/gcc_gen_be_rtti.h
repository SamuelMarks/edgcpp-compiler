/**
 * @file gcc_gen_be_rtti.h
 * @brief RTTI and VTable generation for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_RTTI_H
#define GCC_GEN_BE_RTTI_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Emits a virtual table for a given class type.
 *
 * @param class_type The class type for which to emit the vtable.
 * @param out_vtable Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_vtable(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a Virtual Table Table (VTT) for classes with virtual base classes.
 *
 * @param class_type The class type for which to emit the VTT.
 * @param out_vtt Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a virtual method dispatch thunk.
 *
 * @param thunk_rp The thunk routine pointer to emit.
 * @param out_func Pointer to receive the generated JIT function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_thunk(a_routine_ptr thunk_rp, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a dynamic_cast expression into a call to __dynamic_cast.
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a typeid expression into a type_info descriptor.
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_typeid(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a std::type_info descriptor for the given type.
 *
 * @param tp The type to describe.
 * @param out_descriptor Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_RTTI_H */
