/**
 * @file gcc_gen_be_debug.h
 * @brief Debug Metadata and Scope Generation for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_DEBUG_H
#define GCC_GEN_BE_DEBUG_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Adds debug scope and variable metadata.
 *
 * @param var The frontend variable entity.
 * @param lval The libgccjit lvalue for this variable.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a source location mapping for a statement block.
 *
 * @param stmt The statement node.
 * @param block The corresponding JIT block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_DEBUG_H */
