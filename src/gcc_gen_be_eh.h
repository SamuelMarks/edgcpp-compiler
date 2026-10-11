/**
 * @file gcc_gen_be_eh.h
 * @brief Exception handling and stack unwinding for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_EH_H
#define GCC_GEN_BE_EH_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Ensures that all necessary EH symbols (e.g. __cxa_throw) are declared.
 *
 * @param ctx The libgccjit context.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_declare_eh_symbols(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a throw of an exception.
 *
 * @param expr The throw expression node.
 * @param out_block Pointer to receive the updated basic block or new blocks.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_throw(an_expr_node_ptr expr, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_EH_H */
