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
 * @brief Enables DWARF debug information generation for the compilation context.
 *
 * @details Configures libgccjit to emit DW_TAG_compile_unit and detailed DWARF metadata.
 *
 * @param[in,out] ctx The JIT context to configure.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if ctx is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_enable_debug_info(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Ensures debug tracking is active for the current context.
 *
 * @details Retrieves the active compilation context and enables debug generation options.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an internal error if context retrieval fails.
 */
extern gcc_gen_be_error_t gcc_gen_be_initialize_debug(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits compile unit debug info.
 * @details Synthesizes DW_TAG_compile_unit equivalent by ensuring the primary source file location is initialized.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INTERNAL if the context is missing.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_compile_unit_debug_info(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits subprogram debug info.
 * @details Synthesizes DW_TAG_subprogram equivalent by mapping the routine's source location.
 * @param[in] rout Routine to process.
 * @param[out] func JIT function to associate with debug info.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_subprogram_debug_info(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits type debug info.
 * @details Synthesizes type equivalent debugging info.
 * @param[in] tp Type.
 * @param[out] jit_type JIT type.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *jit_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits variable debug info.
 * @details Synthesizes DW_TAG_variable equivalent by attaching a location to the global or local variable.
 * @param[in] var Variable.
 * @param[out] lval Lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a source location mapping for a statement block.
 * @details Attaches lexical scope boundaries to compound statement blocks.
 * @param[in] stmt The statement node.
 * @param[out] block The corresponding JIT block.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_DEBUG_H */
