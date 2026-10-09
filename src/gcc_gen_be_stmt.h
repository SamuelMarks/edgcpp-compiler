/**
 * @file gcc_gen_be_stmt.h
 * @brief Statement lowering for the GCC backend.
 *
 * This file declares functions for lowering EDG AST statement nodes
 * into libgccjit blocks and operations.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_STMT_H
#define GCC_GEN_BE_STMT_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "statements.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gcc_jit_block gcc_jit_block;
typedef struct gcc_jit_function gcc_jit_function;

/**
 * @brief Retrieves or creates a libgccjit block for a given label.
 *
 * @param func The parent function.
 * @param label The frontend label node.
 * @param out_block A pointer to a gcc_jit_block pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_block` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_label_block(gcc_jit_function *func, a_label_ptr label, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves or creates a libgccjit block for a given switch case entry.
 *
 * @param func The parent function.
 * @param scep The frontend switch case entry.
 * @param out_block A pointer to a gcc_jit_block pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_block` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_switch_case_block(gcc_jit_function *func, a_switch_case_entry_ptr scep, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG statement node into the current libgccjit block.
 *
 * @param stmt The frontend statement node.
 * @param func The parent libgccjit function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_STMT_H */