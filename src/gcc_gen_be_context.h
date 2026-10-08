/**
 * @file gcc_gen_be_context.h
 * @brief Context and core state management for the GCC backend.
 *
 * This file declares the global state (such as the gcc_jit_context and current
 * block) and lifecycle functions for initializing and cleaning up the backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_CONTEXT_H
#define GCC_GEN_BE_CONTEXT_H

#include "gcc_gen_be_error.h"

/* Forward declarations for libgccjit types to avoid including the header in
 * every file that needs the context, though it may still be needed depending
 * on usage. */
typedef struct gcc_jit_context gcc_jit_context;
typedef struct gcc_jit_block gcc_jit_block;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieves the global libgccjit context.
 *
 * @return The current gcc_jit_context pointer, or NULL if not initialized.
 */
extern gcc_jit_context *gcc_gen_be_get_context(void);

/**
 * @brief Sets the global libgccjit context.
 *
 * @param ctx The gcc_jit_context to set as global.
 */
extern void gcc_gen_be_set_context(gcc_jit_context *ctx);

/**
 * @brief Retrieves the currently active libgccjit block.
 *
 * @return The current gcc_jit_block pointer.
 */
extern gcc_jit_block *gcc_gen_be_get_current_block(void);

/**
 * @brief Sets the currently active libgccjit block.
 *
 * @param block The gcc_jit_block to set as current.
 */
extern void gcc_gen_be_set_current_block(gcc_jit_block *block);

/**
 * @struct gcc_gen_be_context_t
 * @brief Encapsulates the global state for the GCC backend.
 *
 * This structure holds the primary libgccjit context, the current
 * active block, and various stacks for control flow and resource cleanup.
 */
typedef struct {
    /** @brief The path to the active translation unit. */
    const char *tu_path;
    
    /** @brief The identifier for the current module. */
    const char *module_id;

    /** @brief Stack of libgccjit blocks to jump to on 'break' in a loop. */
    gcc_jit_block **break_stack;
    
    /** @brief Number of entries in the break stack. */
    size_t break_stack_size;

    /** @brief Stack of libgccjit blocks to jump to on 'continue' in a loop. */
    gcc_jit_block **continue_stack;
    
    /** @brief Number of entries in the continue stack. */
    size_t continue_stack_size;

    /** @brief Stack of libgccjit blocks to jump to on 'break' in a switch. */
    gcc_jit_block **switch_exit_stack;
    
    /** @brief Number of entries in the switch exit stack. */
    size_t switch_exit_stack_size;

    /** @brief Stack for RAII cleanups and destructor invocations. */
    void **cleanup_stack;
    
    /** @brief Number of entries in the cleanup stack. */
    size_t cleanup_stack_size;

    /** @brief Global static initialization function. */
    struct gcc_jit_function *global_ctor_func;
    
    /** @brief Global static initialization block. */
    struct gcc_jit_block *global_ctor_block;
} gcc_gen_be_context_t;

/**
 * @brief Retrieves the global GCC backend context object.
 *
 * @return A pointer to the backend state object.
 */
extern gcc_gen_be_context_t *gcc_gen_be_get_state(void);

/**
 * @brief Configures libgccjit context based on command line options.
 *
 * @param ctx The gcc_jit_context to configure.
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t gcc_gen_be_context_configure_options(gcc_jit_context *ctx);

/**
 * @brief Performs early initialization of the backend, such as loading libraries.
 *
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t gcc_gen_be_early_init(void);

/**
 * @brief Initializes the main gcc_jit_context and sets default options.
 *
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t gcc_gen_be_init(void);

/**
 * @brief Cleans up the gcc_jit_context and associated caches.
 *
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t gcc_gen_be_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_CONTEXT_H */
