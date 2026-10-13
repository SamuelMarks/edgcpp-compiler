/**
 * @file gcc_gen_be_context.c
 * @brief GCC backend context configuration.
 * @details Manages libgccjit context initialization, option propagation, and cleanup.
 */

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
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_GIMPLE, gcc_be_dump_gimple ? 1 : 0);

    /* Position independent code is not exposed as a direct jit option, 
       but can be added via command line arguments to the driver. */
    
    if (gcc_be_dump_rtl_all) {
#ifdef GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE
        gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 1);
#endif
    }
    
    
    if (exceptions_enabled) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
        gcc_jit_context_add_command_line_option(ctx, "-fexceptions");
        gcc_jit_context_add_command_line_option(ctx, "-fasynchronous-unwind-tables");
#endif
    }

    if (gcc_be_ffast_math) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
        gcc_jit_context_add_command_line_option(ctx, "-ffast-math");
#endif
    }
    
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
    /* Optimization levels */
    if (gcc_be_opt_level == 3) gcc_jit_context_add_command_line_option(ctx, "-O3");
    else if (gcc_be_opt_level == 2) gcc_jit_context_add_command_line_option(ctx, "-O2");
    else if (gcc_be_opt_level == 1) gcc_jit_context_add_command_line_option(ctx, "-O1");
    else if (gcc_be_opt_level == 0) gcc_jit_context_add_command_line_option(ctx, "-O0");
    
    /* Target tuning */
    if (gcc_be_march) { char buf[128]; snprintf(buf, sizeof(buf), "-march=%s", gcc_be_march); gcc_jit_context_add_command_line_option(ctx, buf); }
    if (gcc_be_mcpu) { char buf[128]; snprintf(buf, sizeof(buf), "-mcpu=%s", gcc_be_mcpu); gcc_jit_context_add_command_line_option(ctx, buf); }
    if (gcc_be_mtune) { char buf[128]; snprintf(buf, sizeof(buf), "-mtune=%s", gcc_be_mtune); gcc_jit_context_add_command_line_option(ctx, buf); }
    if (gcc_be_mfpu) { char buf[128]; snprintf(buf, sizeof(buf), "-mfpu=%s", gcc_be_mfpu); gcc_jit_context_add_command_line_option(ctx, buf); }

    /* Code models */
    if (gcc_be_mcmodel) { char buf[128]; snprintf(buf, sizeof(buf), "-mcmodel=%s", gcc_be_mcmodel); gcc_jit_context_add_command_line_option(ctx, buf); }

    /* -fPIC / -fPIE */
    if (gcc_be_fPIC) gcc_jit_context_add_command_line_option(ctx, "-fPIC");
    if (gcc_be_fPIE) gcc_jit_context_add_command_line_option(ctx, "-fPIE");
    if (gcc_be_fno_pie) gcc_jit_context_add_command_line_option(ctx, "-fno-pie");

    if (gcc_be_fassociative_math) gcc_jit_context_add_command_line_option(ctx, "-fassociative-math");
    if (gcc_be_fno_trapping_math) gcc_jit_context_add_command_line_option(ctx, "-fno-trapping-math");

    if (gcc_be_m64) gcc_jit_context_add_command_line_option(ctx, "-m64");
    if (gcc_be_m32) gcc_jit_context_add_command_line_option(ctx, "-m32");

#endif

    if (gcc_be_shared) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(ctx, "-shared");
#endif
    }
    if (gcc_be_static) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(ctx, "-static");
#endif
    }
    
    if (C_dialect == C_dialect_cplusplus) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
#if defined(__APPLE__) && defined(__MACH__)
        gcc_jit_context_add_driver_option(ctx, "-lc++");
#else
        gcc_jit_context_add_driver_option(ctx, "-lstdc++");
#endif
#endif
    }

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

    backend_state.thread_id = get_current_thread_id();

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
    
    if (backend_state.break_stack) {
        free(backend_state.break_stack);
        backend_state.break_stack = NULL;
        backend_state.break_stack_size = 0;
        backend_state.break_stack_capacity = 0;
    }

    if (backend_state.continue_stack) {
        free(backend_state.continue_stack);
        backend_state.continue_stack = NULL;
        backend_state.continue_stack_size = 0;
        backend_state.continue_stack_capacity = 0;
    }

    if (backend_state.switch_exit_stack) {
        free(backend_state.switch_exit_stack);
        backend_state.switch_exit_stack = NULL;
        backend_state.switch_exit_stack_size = 0;
        backend_state.switch_exit_stack_capacity = 0;
    }

    if (backend_state.block_stack) {
        free(backend_state.block_stack);
        backend_state.block_stack = NULL;
        backend_state.block_stack_size = 0;
        backend_state.block_stack_capacity = 0;
    }

    if (backend_state.cleanup_stack) {
        free(backend_state.cleanup_stack);
        backend_state.cleanup_stack = NULL;
        backend_state.cleanup_stack_size = 0;
        backend_state.cleanup_stack_capacity = 0;
    }
    
    if (backend_state.scope_stack) {
        /* Any remaining scopes are leaked memory if not popped */
        for (size_t i = 0; i < backend_state.scope_stack_size; ++i) {
            free(backend_state.scope_stack[i]);
        }
        free(backend_state.scope_stack);
        backend_state.scope_stack = NULL;
        backend_state.scope_stack_size = 0;
        backend_state.scope_stack_capacity = 0;
    }
    
    return GCC_GEN_BE_SUCCESS;
}





#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <stdint.h>
#endif
#include <stdlib.h>

/**
 * @brief Gets the current thread ID.
 * @return The thread ID as an unsigned long.
 */
static unsigned long get_current_thread_id(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentThreadId();
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return (unsigned long)tid;
#else
    return (unsigned long)pthread_self();
#endif
}

/**
 * @brief Helper to push to a generic block stack.
 *
 * @details Resizes the given stack if necessary and appends the specified block.
 *
 * @param[in,out] stack Double pointer to the dynamically allocated array of block pointers.
 * @param[in,out] size Pointer to the current number of elements in the stack.
 * @param[in,out] capacity Pointer to the current capacity of the stack.
 * @param[in] block The gcc_jit_block to push onto the stack.
 * @return GCC_GEN_BE_SUCCESS on success, GCC_GEN_BE_ERROR_INVALID_ARGUMENT if block is null, or GCC_GEN_BE_ERROR_OOM.
 */
static gcc_gen_be_error_t push_to_block_stack(gcc_jit_block ***stack, size_t *size, size_t *capacity, gcc_jit_block *block) {
    if (!block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (*size >= *capacity) {
        size_t new_cap = (*capacity == 0) ? 8 : (*capacity * 2);
        void *new_stack = realloc(*stack, new_cap * sizeof(gcc_jit_block *));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
        *stack = (gcc_jit_block **)new_stack;
        *capacity = new_cap;
    }
    (*stack)[(*size)++] = block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Helper to pop from a generic block stack.
 *
 * @details Removes the top element from the stack and returns it.
 *
 * @param[in,out] stack Double pointer to the dynamically allocated array of block pointers.
 * @param[in,out] size Pointer to the current number of elements in the stack.
 * @param[out] out_block Pointer to receive the popped gcc_jit_block.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT on underflow or null pointers.
 */
static gcc_gen_be_error_t pop_from_block_stack(gcc_jit_block ***stack, size_t *size, gcc_jit_block **out_block) {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (*size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT; // Stack underflow
    *out_block = (*stack)[--(*size)];
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_push_break_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.break_stack, &backend_state.break_stack_size, &backend_state.break_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_break_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.break_stack, &backend_state.break_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_push_continue_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.continue_stack, &backend_state.continue_stack_size, &backend_state.continue_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_continue_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.continue_stack, &backend_state.continue_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_push_switch_exit_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.switch_exit_stack, &backend_state.switch_exit_stack_size, &backend_state.switch_exit_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_switch_exit_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.switch_exit_stack, &backend_state.switch_exit_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_push_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.block_stack, &backend_state.block_stack_size, &backend_state.block_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.block_stack, &backend_state.block_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_check_thread_safety(void) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.thread_id != get_current_thread_id()) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_INTERNAL, "Thread safety violation in gcc_gen_be context");
    }
    return GCC_GEN_BE_SUCCESS;
}

typedef void (*gcc_gen_be_cleanup_func_t)(void *);

/** @brief A cleanup entry */
typedef struct {
    /** @brief function */
    gcc_gen_be_cleanup_func_t func;
    /** @brief data */
    void *data;
} gcc_gen_be_cleanup_entry_t;

/**
 * @brief Pushes a cleanup function to the RAII stack.
 */
gcc_gen_be_error_t gcc_gen_be_push_cleanup(gcc_gen_be_cleanup_func_t func, void *data) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (backend_state.cleanup_stack_size >= backend_state.cleanup_stack_capacity) {
        size_t new_cap = (backend_state.cleanup_stack_capacity == 0) ? 16 : (backend_state.cleanup_stack_capacity * 2);
        void *new_stack = realloc(backend_state.cleanup_stack, new_cap * sizeof(gcc_gen_be_cleanup_entry_t));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
        backend_state.cleanup_stack = (void **)new_stack;
        backend_state.cleanup_stack_capacity = new_cap;
    }

    gcc_gen_be_cleanup_entry_t *entries = (gcc_gen_be_cleanup_entry_t *)backend_state.cleanup_stack;
    entries[backend_state.cleanup_stack_size].func = func;
    entries[backend_state.cleanup_stack_size].data = data;
    backend_state.cleanup_stack_size++;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all pending cleanups in reverse order and clears the stack.
 */
gcc_gen_be_error_t gcc_gen_be_execute_cleanups(void) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_cleanup_entry_t *entries = (gcc_gen_be_cleanup_entry_t *)backend_state.cleanup_stack;
    while (backend_state.cleanup_stack_size > 0) {
        backend_state.cleanup_stack_size--;
        if (entries[backend_state.cleanup_stack_size].func) {
            entries[backend_state.cleanup_stack_size].func(entries[backend_state.cleanup_stack_size].data);
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/** @brief A scope entry */
/**
 * @brief Represents a single registered cleanup.
 */
typedef struct gcc_gen_be_cleanup {
    /** @brief The variable to clean up. */
    struct gcc_jit_lvalue *var;
    /** @brief The destructor function. */
    struct gcc_jit_function *dtor_func;
    /** @brief Next cleanup in list. */
    struct gcc_gen_be_cleanup *next;
} gcc_gen_be_cleanup_t;

/**
 * @brief Represents a block scope with cleanups.
 */
typedef struct gcc_gen_be_scope {
    /** @brief The frontend lifetime object. */
    an_object_lifetime_ptr lifetime;
    /** @brief List of cleanups registered in this scope. */
    gcc_gen_be_cleanup_t *cleanups;
} gcc_gen_be_scope_t;

gcc_gen_be_error_t gcc_gen_be_push_scope(an_object_lifetime_ptr lifetime) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size >= backend_state.scope_stack_capacity) {
        size_t new_cap = (backend_state.scope_stack_capacity == 0) ? 16 : (backend_state.scope_stack_capacity * 2);
        void *new_stack = realloc(backend_state.scope_stack, new_cap * sizeof(gcc_gen_be_scope_t *));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
        backend_state.scope_stack = (struct gcc_gen_be_scope **)new_stack;
        backend_state.scope_stack_capacity = new_cap;
    }
    
    gcc_gen_be_scope_t *new_scope = (gcc_gen_be_scope_t *)malloc(sizeof(gcc_gen_be_scope_t));
    if (!new_scope) return GCC_GEN_BE_ERROR_OOM;
    new_scope->lifetime = lifetime;
    new_scope->cleanups = NULL;
    
    backend_state.scope_stack[backend_state.scope_stack_size++] = (struct gcc_gen_be_scope *)new_scope;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Registers a cleanup routine for a local variable.
 * @details Adds the specified destructor function to the current scope's cleanup list.
 * @param[in] var The variable to clean up.
 * @param[in] dtor_func The destructor function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_register_cleanup(struct gcc_jit_lvalue *var, struct gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size == 0) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_gen_be_scope_t *top_scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
    
    gcc_gen_be_cleanup_t *cleanup = (gcc_gen_be_cleanup_t *)malloc(sizeof(gcc_gen_be_cleanup_t));
    if (!cleanup) return GCC_GEN_BE_ERROR_OOM;
    
    cleanup->var = var;
    cleanup->dtor_func = dtor_func;
    /* Push to front so they are executed in reverse order */
    cleanup->next = top_scope->cleanups;
    top_scope->cleanups = cleanup;
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits registered cleanups up to a target lifetime.
 * @details Emits destructor calls for all variables in scopes up to the target lifetime.
 * @param[in] target_lifetime The lifetime to stop emitting cleanups for (NULL for all).
 * @param[in] block The block to emit cleanups into.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_cleanups(an_object_lifetime_ptr target_lifetime, struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (!block) return GCC_GEN_BE_SUCCESS; /* Nothing to emit into */

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));

    /* Traverse from top down to (but not including) the scope with target_lifetime */
    for (size_t i = backend_state.scope_stack_size; i > 0; --i) {
        gcc_gen_be_scope_t *scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[i - 1];
        
        /* If we hit the target lifetime or its ancestor, we stop. 
           Wait, a target_lifetime might not exactly match our scope's lifetime pointer if EDG creates
           multiple overlapping ones. But typically, the innermost lifetime in goto/break EXACTLY matches
           one of the active block scopes. */
        /* To be safe, we also check if we reached the target. */
        
        if (target_lifetime != NULL) {
            /* Check if scope->lifetime is target_lifetime or an ancestor. 
               We just check equality for now. */
            an_object_lifetime_ptr curr = scope->lifetime;
            int found = 0;
            while (curr) {
                if (curr == target_lifetime) {
                    found = 1;
                    break;
                }
                curr = curr->parent_lifetime;
            }
            if (found) break; /* stop traversing */
        }
        
        gcc_gen_be_cleanup_t *cleanup = scope->cleanups;
        while (cleanup) {
            gcc_jit_rvalue *arg = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(cleanup->var, NULL), void_ptr_type);
            gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 1, &arg));
            cleanup = cleanup->next;
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Pops the current cleanup scope.
 * @details Decrements the scope depth and cleans up the scope structure.
 * @param[in] block The block to emit cleanups into before popping.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_pop_scope(struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size == 0) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT; /* Underflow */
    }
    
    gcc_gen_be_scope_t *top_scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
    
    /* Emit cleanups into block if provided */
    if (block) {
        gcc_jit_context *ctx = NULL;
        if (gcc_gen_be_get_context(&ctx) == GCC_GEN_BE_SUCCESS) {
            gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
            gcc_gen_be_cleanup_t *cleanup = top_scope->cleanups;
            while (cleanup) {
                gcc_jit_rvalue *arg = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(cleanup->var, NULL), void_ptr_type);
                gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 1, &arg));
                cleanup = cleanup->next;
            }
        }
    }
    
    gcc_gen_be_cleanup_t *cleanup = top_scope->cleanups;
    while (cleanup) {
        gcc_gen_be_cleanup_t *next = cleanup->next;
        free(cleanup);
        cleanup = next;
    }
    
    backend_state.scope_stack_size--;
    free(top_scope);
    
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
