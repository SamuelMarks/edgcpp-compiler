/**
 * @file gcc_gen_be_context.c
 * @brief Implementation of context and core state management.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_cache.h"
#include "cmd_line.h"
#include <libgccjit.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

static gcc_jit_context *gcc_jit_ctx = NULL;
static gcc_jit_block *current_block = NULL;
static gcc_gen_be_context_t backend_state = {0};

/**
 * @brief Retrieves the global GCC backend context object.
 *
 * @param out_state Pointer to receive the backend state object.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_state(gcc_gen_be_context_t **out_state) GCC_GEN_BE_NOEXCEPT {
    if (!out_state) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_state = &backend_state;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active libgccjit context.
 *
 * @param out_ctx Pointer to receive the gcc_jit_context pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_context(gcc_jit_context **out_ctx) GCC_GEN_BE_NOEXCEPT {
    if (!out_ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ctx = gcc_jit_ctx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the currently active libgccjit context.
 *
 * @param ctx The gcc_jit_context to set as active.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_set_context(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_ctx = ctx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active libgccjit block.
 *
 * @param out_block Pointer to receive the gcc_jit_block pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_current_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = current_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the currently active libgccjit block.
 *
 * @param block The gcc_jit_block to set as active.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_set_current_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    current_block = block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Performs early initialization of the GCC backend.
 *
 * Loads the libgccjit dynamic library for the current platform.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_error_t err;
#if defined(_WIN32)
    err = load_libgccjit_windows();
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
    err = load_libgccjit_posix();
#else
    err = GCC_GEN_BE_SUCCESS;
#endif
    return err;
}

/**
 * @brief Configures libgccjit context based on command line options.
 *
 * Applies optimization level, debug info flags, and other options
 * parsed from the EDG command line.
 *
 * @param ctx The gcc_jit_context to configure.
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_context_configure_options(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    if (!ctx) return GCC_GEN_BE_ERROR_NULL_POINTER;

    /* Set optimization level */
    gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, gcc_be_opt_level);

    /* Set debug info */
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, gcc_be_debug_info ? 1 : 0);

    /* Set dump options */
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_TREE, gcc_be_dump_initial_tree ? 1 : 0);
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_GIMPLE, gcc_be_dump_gimple ? 1 : 0);

    /* Position independent code is not exposed as a direct jit option, 
       but can be added via command line arguments to the driver. */
    if (gcc_be_fPIC || gcc_be_fPIE) {
        gcc_jit_context_add_command_line_option(ctx, gcc_be_fPIE ? "-fPIE" : "-fPIC");
    }

    /* Bind trace logfile if diagnostics are on and tracing is requested */
    /* Wait, EDG doesn't have a direct flag for trace logfile in our added options, 
       but we can use standard output or a file if requested. For now, leave empty or 
       check if we want to trace. */
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes the main gcc_jit_context and its dependencies.
 *
 * Acquires a new libgccjit context and configures it.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_init(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_ctx = gcc_jit_context_acquire();
    if (!gcc_jit_ctx) {
        return GCC_GEN_BE_ERROR_OOM;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_context_configure_options(gcc_jit_ctx));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Cleans up the gcc_jit_context and associated caches.
 *
 * Releases the main context and clears internal backend caches.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_cleanup(void) GCC_GEN_BE_NOEXCEPT {
    if (gcc_jit_ctx) {
        gcc_jit_context_release(gcc_jit_ctx);
        gcc_jit_ctx = NULL;
    }
    
    GCC_GEN_BE_CHECK(cache_clear_all());
    
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
