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

struct gcc_jit_block;
struct gcc_jit_function;

BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves or creates a libgccjit block for a given label.
 *
 * @param func The parent function.
 * @param label The frontend label node.
 * @param out_block A pointer to a gcc_jit_block pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_block` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_label_block(struct gcc_jit_function *func, a_label_ptr label, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves or creates a libgccjit block for a given switch case entry.
 *
 * @param func The parent function.
 * @param scep The frontend switch case entry.
 * @param out_block A pointer to a gcc_jit_block pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_block` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_switch_case_block(struct gcc_jit_function *func, a_switch_case_entry_ptr scep, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG statement node into the current libgccjit block.
 *
 * @param stmt The frontend statement node.
 * @param func The parent libgccjit function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

/**
 * @brief Emits a __builtin_stack_save call and saves the result in a local variable.
 */
/**
 * @brief Emits a stack save operation.
 * @details Emits a call to __builtin_stack_save.
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[out] out_save Pointer to receive the saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_stack_save(struct gcc_jit_function *func, struct gcc_jit_block *block, struct gcc_jit_lvalue **out_save) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a __builtin_stack_restore call using the previously saved stack pointer.
 */
/**
 * @brief Emits a stack restore operation.
 * @details Emits a call to __builtin_stack_restore.
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[in] save_var The saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_stack_restore(struct gcc_jit_function *func, struct gcc_jit_block *block, struct gcc_jit_lvalue *save_var) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers coroutine frame.
 * @details Stubs coroutine frame.
 * @param[in] rout Routine.
 * @param[out] out_frame_type Frame type.
 * @return GCC_GEN_BE_SUCCESS
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_frame(a_routine_ptr rout, struct gcc_jit_type **out_frame_type) GCC_GEN_BE_NOEXCEPT;
/**
 * @brief Lowers coroutine ramp.
 * @details Stubs coroutine ramp.
 * @param[in] rout Routine.
 * @param[out] ramp_func Ramp function.
 * @return GCC_GEN_BE_SUCCESS
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr rout, struct gcc_jit_function *ramp_func) GCC_GEN_BE_NOEXCEPT;
/**
 * @brief Lowers coroutine dispatchers.
 * @details Stubs coroutine dispatchers.
 * @param[in] rout Routine.
 * @param[out] out_resume Resume fn.
 * @param[out] out_destroy Destroy fn.
 * @return GCC_GEN_BE_SUCCESS
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_dispatchers(a_routine_ptr rout, struct gcc_jit_function **out_resume, struct gcc_jit_function **out_destroy) GCC_GEN_BE_NOEXCEPT;

#endif
