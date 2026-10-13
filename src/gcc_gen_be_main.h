/**
 * @file gcc_gen_be_main.h
 * @brief Main entry points for the GCC backend.
 *
 * This file declares the primary entry points (`gcc_gen_be` and `back_end`)
 * that interface directly with the EDG frontend's execution flow.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_MAIN_H
#define GCC_GEN_BE_MAIN_H

#include "gcc_gen_be_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Early initialization for the GCC backend.
 *
 * This function is called by the front end before command-line options
 * are processed.
 */
/** @return GCC_GEN_BE_SUCCESS on success. */
/** @return GCC_GEN_BE_SUCCESS on success. */
extern gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Main entry point for the GCC backend.
 *
 * It orchestrates the translation of the EDG AST into libgccjit constructs
 * and triggers compilation.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Hook called by the EDG frontend to execute the backend.
 *
 * This is the standard entry point expected by the EDG frontend architecture.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Parses and applies command-line options to the given context.
 *
 * @param ctx The libgccjit context.
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_command_line_options(struct gcc_jit_context *ctx, int argc, char **argv) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Manages Multi-TU compilation by isolating contexts.
 *
 * @param source_files Array of source files to process.
 * @param num_files Number of source files.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_compile_multi_tu(const char **source_files, int num_files, int argc, char **argv) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits the final output artifact (assembler, object, shared library, executable).
 *
 * @param ctx The context.
 * @param output_kind Target kind string (e.g. "asm", "obj", "dll", "exe").
 * @param output_path Target file path.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_artifact(struct gcc_jit_context *ctx, const char *output_kind, const char *output_path) GCC_GEN_BE_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_MAIN_H */