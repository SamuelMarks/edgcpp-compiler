/**
 * @file gcc_gen_be_eh.c
 * @brief Implementation of Exception handling and stack unwinding.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_stmt.h"


#if BACK_END_IS_GCC_GEN_BE

/** @brief EH region. */
/** @brief EH region. */
typedef struct gcc_gen_be_eh_region {
    /** @brief Try ID. */
    /** @brief Try ID. */
    int try_id;
    /** @brief LP ID. */
    /** @brief LP ID. */
    int landing_pad_id;
    /** @brief Action table index. */
    /** @brief Action table index. */
    int action_table_index;
    /** @brief Next region. */
    /** @brief Next region. */
    struct gcc_gen_be_eh_region *next;
} gcc_gen_be_eh_region_t;

/** @brief EH type filter. */
/** @brief EH type filter. */
typedef struct gcc_gen_be_eh_type_filter {
    /** @brief Type. */
    /** @brief Type. */
    a_type_ptr type;
    /** @brief Type ID. */
    /** @brief Type ID. */
    int type_id;
    /** @brief Next filter. */
    /** @brief Next filter. */
    struct gcc_gen_be_eh_type_filter *next;
} gcc_gen_be_eh_type_filter_t;

/** @brief EH state structure. */
/** @brief EH state structure. */
typedef struct gcc_gen_be_eh_state {
/** @brief current_func member. */
    gcc_jit_function *current_func;
    /** @brief Current function ID. */
    /** @brief Current function ID. */
    int current_func_id;
    /** @brief Next try ID. */
    /** @brief Next try ID. */
    int next_try_id;
    /** @brief Next action ID. */
    /** @brief Next action ID. */
    int next_action_id;
    /** @brief Next type ID. */
    /** @brief Next type ID. */
    int next_type_id;
/** @brief lp_start_emitted member. */
    a_boolean lp_start_emitted;
    
    /** @brief Regions. */
    /** @brief Regions. */
    gcc_gen_be_eh_region_t *regions;
    /** @brief Types. */
    /** @brief Types. */
    gcc_gen_be_eh_type_filter_t *types;
    
/** @brief action_table member. */
    char action_table[4096];
    /** @brief Action table length. */
    /** @brief Action table length. */
    int action_table_len;
} gcc_gen_be_eh_state_t;

/** @brief eh_state member. */
static gcc_gen_be_eh_state_t eh_state = {0};
/** @brief Global function counter for LP uniqueness. */
/** @brief Global function counter for LP uniqueness. */
static int global_func_counter = 0;

/**
 * @brief reset_eh_state function.
 * @details Resets the internal EH state for a new function.
 * @param[in] func The JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
static gcc_gen_be_error_t reset_eh_state(gcc_jit_function *func) {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    eh_state.current_func = func;
    eh_state.current_func_id = ++global_func_counter;
    eh_state.next_try_id = 1;
    eh_state.next_action_id = 1;
    eh_state.next_type_id = 1;
    eh_state.lp_start_emitted = FALSE;
    
    while (eh_state.regions) {
        gcc_gen_be_eh_region_t *r = eh_state.regions;
        eh_state.regions = r->next;
        free(r);
    }
    while (eh_state.types) {
        gcc_gen_be_eh_type_filter_t *t = eh_state.types;
        eh_state.types = t->next;
        free(t);
    }
    eh_state.action_table_len = 0;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief get_type_filter_index function.
 * @details Retrieves or creates a type filter index for the action table.
 * @param[in] type The EH filter type.
 * @param[out] out_index The returned filter index.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_type_filter_index(a_type_ptr type, int *out_index) {
    if (!out_index) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (!type) {
        *out_index = 0; /* catch-all */
        return GCC_GEN_BE_SUCCESS;
    }
    
    for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t; t = t->next) {
        if (t->type == type) {
            *out_index = t->type_id;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    
    gcc_gen_be_eh_type_filter_t *t = (gcc_gen_be_eh_type_filter_t *)malloc(sizeof(gcc_gen_be_eh_type_filter_t));
    if (!t) return GCC_GEN_BE_ERROR_OOM;
    t->type = type;
    t->type_id = eh_state.next_type_id++;
    t->next = eh_state.types;
    eh_state.types = t;
    *out_index = t->type_id;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief emit_eh_tables function.
 * @details Emits the LSDA exception handling tables.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t emit_eh_tables() {
    if (!eh_state.current_func || !eh_state.regions) return GCC_GEN_BE_SUCCESS;
    
    /* We must emit the LSDA table at the end of the function.
       Since we can't emit it "outside" easily via libgccjit, we append an extended asm
       to a dummy block or the last block. */
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_block *table_block = gcc_jit_function_new_block(eh_state.current_func, "eh_table_block");
    
    char asm_buf[8192];
    int offset = 0;
    
    offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset,
        ".pushsection .gcc_except_table,\"a\",@progbits\n"
        ".align 4\n"
        ".Lexception_table_%d:\n"
        ".byte 0x1b\n" /* LPStart encoding: pcrel | sdata4 */
        ".long .Llp_start_%d - .\n"
        ".byte 0x00\n" /* TType encoding: absptr (for simplicity we can use absptr or pcrel, let's use 0x00 absptr) */
        ".uleb128 .LTTbase_%d - .LTTbaseref_%d\n"
        ".LTTbaseref_%d:\n"
        ".byte 0x01\n" /* Call site encoding: uleb128 */
        ".uleb128 .Lcallsite_end_%d - .Lcallsite_start_%d\n"
        ".Lcallsite_start_%d:\n",
        eh_state.current_func_id, eh_state.current_func_id,
        eh_state.current_func_id, eh_state.current_func_id,
        eh_state.current_func_id,
        eh_state.current_func_id, eh_state.current_func_id,
        eh_state.current_func_id
    );
    
    /* Write Call Site Table */
    for (gcc_gen_be_eh_region_t *r = eh_state.regions; r; r = r->next) {
        offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset,
            ".uleb128 .Ltry_start_%d_%d - .Llp_start_%d\n"
            ".uleb128 .Ltry_end_%d_%d - .Ltry_start_%d_%d\n"
            ".uleb128 .Llanding_pad_%d_%d - .Llp_start_%d\n"
            ".uleb128 %d\n",
            eh_state.current_func_id, r->try_id, eh_state.current_func_id,
            eh_state.current_func_id, r->try_id, eh_state.current_func_id, r->try_id,
            eh_state.current_func_id, r->landing_pad_id, eh_state.current_func_id,
            r->action_table_index
        );
    }
    
    offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset,
        ".Lcallsite_end_%d:\n", eh_state.current_func_id
    );
    
    /* Write Action Table */
    for (int i = 0; i < eh_state.action_table_len; ++i) {
        asm_buf[offset++] = eh_state.action_table[i];
    }
    
    /* Write Type Table */
    offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset,
        ".align 4\n"
    );
    
    /* Types are written in reverse order of their ID, since they are indexed negatively from TType base */
    int num_types = eh_state.next_type_id - 1;
    a_type_ptr *types_array = (a_type_ptr *)malloc(num_types * sizeof(a_type_ptr));
    for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t; t = t->next) {
        types_array[t->type_id - 1] = t->type;
    }
    
    for (int i = num_types - 1; i >= 0; --i) {
        if (types_array[i] == NULL) {
            /* catch-all */
            offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset, ".long 0\n");
        } else {
            /* Get mangled name for typeinfo */
            gcc_jit_lvalue *tinfo = NULL;
            gcc_gen_be_emit_type_descriptor(types_array[i], &tinfo);
            const char *tinfo_name = gcc_jit_object_get_debug_string(gcc_jit_lvalue_as_object(tinfo));
            offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset, ".long %s\n", tinfo_name);
        }
    }
    free(types_array);
    
    offset += snprintf(asm_buf + offset, sizeof(asm_buf) - offset,
        ".LTTbase_%d:\n"
        ".popsection\n",
        eh_state.current_func_id
    );
    
    gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(table_block, NULL, asm_buf);
    gcc_jit_extended_asm_set_volatile_flag(ext_asm, 1);
    
    gcc_jit_block_end_with_void_return(table_block, NULL); /* or unreachable */
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Declares an exception handling runtime function.
 *
 * @details Adds the function declaration to the given GCC JIT context.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] name The name of the function.
 * @param[in] ret_type The return type.
 * @param[in] num_params The number of parameters.
 * @param[in] param_types The array of parameter types.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t declare_eh_func(gcc_jit_context *ctx, const char *name, gcc_jit_type *ret_type, int num_params, gcc_jit_type **param_types) {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)name, &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) return GCC_GEN_BE_SUCCESS;

    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc(num_params * sizeof(gcc_jit_param *));
        if (!params) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < num_params; ++i) {
            params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], "p");
        }
    }

    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, name, num_params, params, 0);
    if (params) free(params);

    return cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)name, fn);

gcc_gen_be_error_t gcc_gen_be_begin_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(reset_eh_state(func));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_end_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (eh_state.current_func == func) {
        GCC_GEN_BE_CHECK(emit_eh_tables());
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_declare_eh_symbols(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

    /* __gxx_personality_v0 */
    /* Libgccjit doesn't have native personality settings exposed easily,
       but we must declare the symbol. */
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *uint64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
    gcc_jit_type *personality_params[] = { int_type, int_type, uint64_type, void_ptr_type, void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gxx_personality_v0", int_type, 5, personality_params));


    /* void *__cxa_allocate_exception(size_t thrown_size) */
    gcc_jit_type *p1[] = { size_t_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_allocate_exception", void_ptr_type, 1, p1));

    /* void __cxa_throw(void *thrown_exception, struct std::type_info *tinfo, void (*dest)(void *)) */
    gcc_jit_type *p2[] = { void_ptr_type, void_ptr_type, void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_throw", void_type, 3, p2));

    /* void *__cxa_begin_catch(void *exceptionObject) */
    gcc_jit_type *p3[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_begin_catch", void_ptr_type, 1, p3));

    /* void __cxa_end_catch() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_end_catch", void_type, 0, NULL));

    
    /* void _Unwind_Resume(struct _Unwind_Exception *) */
    gcc_jit_type *p4[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "_Unwind_Resume", void_type, 1, p4));

    /* void __cxa_call_unexpected(void *) */
    gcc_jit_type *p5[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_call_unexpected", void_type, 1, p5));

    /* void std::terminate() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "_ZSt9terminatev", void_type, 0, NULL));

    /* void __cxa_rethrow() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_rethrow", void_type, 0, NULL));

    return GCC_GEN_BE_SUCCESS;

gcc_gen_be_error_t gcc_gen_be_emit_throw(an_expr_node_ptr expr, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_throw_supplement_ptr tsp = expr->variant.throw_info;
    if (tsp) {
        /* Throw of an object. */
        a_type_ptr throw_type = tsp->type;
        long sz = (long)size_of_type(throw_type);

        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

        gcc_jit_param *alloc_param = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
        gcc_jit_function *alloc_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__cxa_allocate_exception", 1, &alloc_param, 0);

        gcc_jit_rvalue *size_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, sz);
        gcc_jit_rvalue *exc_buf = gcc_jit_context_new_call(ctx, NULL, alloc_fn, 1, &size_rval);

        /* Emit assignment of thrown value into allocated exception buffer. */
        a_dynamic_init_ptr dip = tsp->dynamic_init;
        if (dip && dip->kind == dik_expression) {
            gcc_jit_rvalue *val_rval = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &val_rval));
            if (val_rval) {
                gcc_jit_type *exc_type = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(throw_type, &exc_type));
                gcc_jit_lvalue *exc_deref = gcc_jit_rvalue_dereference(
                    gcc_jit_context_new_cast(ctx, NULL, exc_buf, gcc_jit_type_get_pointer(exc_type)), NULL);
                gcc_jit_block_add_assignment(cblock, NULL, exc_deref, val_rval);
            }
        }

        gcc_jit_param *throw_params[3];
        throw_params[0] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "exc");
        throw_params[1] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "tinfo");
        throw_params[2] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dest");
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *throw_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_throw", 3, throw_params, 0);

        gcc_jit_rvalue *args[3];
        args[0] = exc_buf;

        /* Phase 3: typeinfo_ptr */
        gcc_jit_lvalue *type_info_lval = NULL;
        err = gcc_gen_be_emit_type_descriptor(throw_type, &type_info_lval);
        if (err != GCC_GEN_BE_SUCCESS) return err;
        gcc_jit_rvalue *type_info_addr = gcc_jit_lvalue_get_address(type_info_lval, NULL);
        args[1] = gcc_jit_context_new_cast(ctx, NULL, type_info_addr, void_ptr_type);

        /* Phase 3: destructor_ptr */
        gcc_jit_rvalue *dtor_rval = gcc_jit_context_null(ctx, void_ptr_type);
        if (dip && dip->destructor) {
            void *cached = NULL;
            err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, dip->destructor, &cached);
            if (err == GCC_GEN_BE_SUCCESS && cached) {
                gcc_jit_function *dtor_fn = (gcc_jit_function *)cached;
                dtor_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(dtor_fn, NULL), void_ptr_type);
            }
        }
        args[2] = dtor_rval;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, cblock));



        gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, throw_fn, 3, args));
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, cblock));

        /* Rethrow */

        gcc_jit_function *rethrow_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_rethrow", 0, NULL, 0);
        gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, rethrow_fn, 0, NULL));
    }

    /* Control flow terminated after throw */
    gcc_jit_block *unreachable_block = gcc_jit_function_new_block(gcc_jit_block_get_function(cblock), "unreachable_after_throw");
    gcc_jit_block_end_with_jump(cblock, NULL, unreachable_block);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(unreachable_block));
    
    *out_block = unreachable_block;
    return GCC_GEN_BE_SUCCESS;



static gcc_gen_be_error_t get_eh_frame_type(gcc_jit_context *ctx, gcc_jit_type **out_type) {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        *out_type = (gcc_jit_type *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 64);
    
    gcc_jit_field *f_next = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "next");
    gcc_jit_field *f_jmpbuf = gcc_jit_context_new_field(ctx, NULL, array_type, "jmpbuf");
    gcc_jit_field *f_exc = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "exception_object");
    gcc_jit_field *f_tinfo = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "type_info");
    
    gcc_jit_field *fields[] = { f_next, f_jmpbuf, f_exc, f_tinfo };
    gcc_jit_struct *eh_struct = gcc_jit_context_new_struct_type(ctx, NULL, "__eh_frame", 4, fields);
    
    gcc_jit_type *eh_type = gcc_jit_struct_as_type(eh_struct);
    cache_insert(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", eh_type);
    *out_type = eh_type;
    return GCC_GEN_BE_SUCCESS;

static gcc_gen_be_error_t get_eh_chain(gcc_jit_context *ctx, gcc_jit_lvalue **out_chain) {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_eh_chain", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        *out_chain = (gcc_jit_lvalue *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_lvalue *chain = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, void_ptr_type, "__gcc_be_eh_chain");
    if (p_gcc_jit_lvalue_set_tls_model) {
        p_gcc_jit_lvalue_set_tls_model(chain, GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC);
    }
    
    cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_eh_chain", chain);
    *out_chain = chain;
    return GCC_GEN_BE_SUCCESS;

static gcc_gen_be_error_t declare_cxa_type_match(gcc_jit_context *ctx, gcc_jit_function **out_func) {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_type_match", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        *out_func = (gcc_jit_function *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *params[] = {
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "catch_type"),
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "throw_type"),
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "exc_ptr")
    };
    
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__cxa_type_match", 3, params, 0);
    cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_type_match", fn);
    *out_func = fn;
    return GCC_GEN_BE_SUCCESS;

gcc_gen_be_error_t gcc_gen_be_lower_try_block(a_statement_ptr stmt, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    
    int try_id = eh_state.next_try_id++;
    int lp_id = try_id; /* for simplicity */
    
    if (!eh_state.lp_start_emitted) {
        char buf[128];
        snprintf(buf, sizeof(buf), ".Llp_start_%d:", eh_state.current_func_id);
        gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(cblock, NULL, buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
        eh_state.lp_start_emitted = TRUE;
    }
    
    char try_start_buf[128];
    snprintf(try_start_buf, sizeof(try_start_buf), ".Ltry_start_%d_%d:", eh_state.current_func_id, try_id);
    gcc_jit_extended_asm *asm_start = gcc_jit_block_add_extended_asm(cblock, NULL, try_start_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_start, 1);

    gcc_jit_block *try_body_block = gcc_jit_function_new_block(func, "try_body");
    gcc_jit_block *catch_dispatch_block = gcc_jit_function_new_block(func, "catch_dispatch");
    gcc_jit_block *end_try_block = gcc_jit_function_new_block(func, "end_try");

    #ifdef GCC_GEN_BE_USE_SJLJ_EXCEPTIONS
    /* Architecture SjLj Exception Handling Fallback */
    gcc_jit_type *eh_type = NULL;
    void *cached_eh_type = NULL;
    cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_type);
    if (cached_eh_type) {
        eh_type = (gcc_jit_type *)cached_eh_type;
        gcc_jit_lvalue *sjlj_frame = gcc_jit_function_new_local(func, NULL, eh_type, "sjlj_frame");
        gcc_jit_lvalue *chain = NULL;
        get_eh_chain(ctx, &chain);
        
        /* sjlj_frame.next = *chain */
        gcc_jit_field *f_next = gcc_jit_type_get_field(eh_type, 0); /* next is index 0 */
        gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next);
        gcc_jit_block_add_assignment(cblock, NULL, sjlj_next, gcc_jit_lvalue_as_rvalue(chain));
        
        /* *chain = &sjlj_frame */
        gcc_jit_block_add_assignment(cblock, NULL, chain, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(sjlj_frame, NULL), void_ptr_type));
        
        /* Call setjmp */
        void *cached_setjmp = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp);
        if (!cached_setjmp) {
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn);
            cached_setjmp = setjmp_fn;
        }
        
        gcc_jit_field *f_jmpbuf = gcc_jit_type_get_field(eh_type, 1); /* jmpbuf is index 1 */
        gcc_jit_rvalue *jmpbuf_addr = gcc_jit_lvalue_get_address(gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_jmpbuf), NULL);
        gcc_jit_rvalue *setjmp_call = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_addr);
        
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
        gcc_jit_rvalue *is_zero = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, setjmp_call, zero);
        
        gcc_jit_block_end_with_conditional(cblock, NULL, is_zero, try_body_block, catch_dispatch_block);
    } else {
        gcc_jit_block_end_with_jump(cblock, NULL, try_body_block);
    }
#else
    gcc_jit_block_end_with_jump(cblock, NULL, try_body_block);
#endif

    /* Normal execution */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(try_body_block));

    a_try_supplement_ptr tsp = stmt->variant.try_block;
    if (tsp && tsp->statement) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(tsp->statement, func));
    }

    gcc_jit_block *after_try = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_try));
    if (after_try) {
#ifdef GCC_GEN_BE_USE_SJLJ_EXCEPTIONS
        void *cached_eh_type = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_type);
        if (cached_eh_type) {
            gcc_jit_type *eh_type = (gcc_jit_type *)cached_eh_type;
            gcc_jit_lvalue *chain = NULL;
            get_eh_chain(ctx, &chain);
            gcc_jit_rvalue *chain_val = gcc_jit_lvalue_as_rvalue(chain);
            gcc_jit_rvalue *chain_deref = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, chain_val, gcc_jit_type_get_pointer(eh_type)), NULL));
            gcc_jit_rvalue *next_val = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, chain_val, gcc_jit_type_get_pointer(eh_type)), NULL), NULL, gcc_jit_type_get_field(eh_type, 0)));
            gcc_jit_block_add_assignment(after_try, NULL, chain, next_val);
        }
#endif
        char try_end_buf[128];
        snprintf(try_end_buf, sizeof(try_end_buf), ".Ltry_end_%d_%d:", eh_state.current_func_id, try_id);
        gcc_jit_extended_asm *asm_end = gcc_jit_block_add_extended_asm(after_try, NULL, try_end_buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_end, 1);
        gcc_jit_block_end_with_jump(after_try, NULL, end_try_block);
    }

    /* Exception caught (Landing Pad) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_dispatch_block));
    
    char lp_buf[128];
    snprintf(lp_buf, sizeof(lp_buf), ".Llanding_pad_%d_%d:", eh_state.current_func_id, lp_id);
    gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(catch_dispatch_block, NULL, lp_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
    
    /* Construct Action Table */
    int action_idx = 0;
    a_handler_ptr h = tsp->handlers;
    
    /* We encode actions backwards, so we collect them first */
    int num_handlers = 0;
    while (h) { num_handlers++; h = h->next; }
    
    if (num_handlers > 0) {
        a_type_ptr *handler_types = (a_type_ptr *)malloc(num_handlers * sizeof(a_type_ptr));
        h = tsp->handlers;
        for (int i = 0; i < num_handlers; i++) {
            handler_types[i] = h->parameter ? h->parameter->type : NULL;
            h = h->next;
        }
        
        int prev_action_offset = 0;
        int first_action_offset = 0;
        /* LEB128 encoding logic is simplified since action offset is typically small, we just emit 1-2 bytes */
        for (int i = num_handlers - 1; i >= 0; i--) {
            int filter_id = 0;
            gcc_gen_be_error_t err = get_type_filter_index(handler_types[i], &filter_id);
            if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
            int action_offset = eh_state.action_table_len;
            
            /* Type filter index (sleb128) */
            if (filter_id < 64) {
                eh_state.action_table[eh_state.action_table_len++] = filter_id & 0x7f;
            } else {
                eh_state.action_table[eh_state.action_table_len++] = (filter_id & 0x7f) | 0x80;
                eh_state.action_table[eh_state.action_table_len++] = (filter_id >> 7) & 0x7f;
            }
            
            /* Next action offset (sleb128) */
            int next_offset = prev_action_offset ? (prev_action_offset - action_offset) : 0;
            if (next_offset < 64) {
                eh_state.action_table[eh_state.action_table_len++] = next_offset & 0x7f;
            } else {
                eh_state.action_table[eh_state.action_table_len++] = (next_offset & 0x7f) | 0x80;
                eh_state.action_table[eh_state.action_table_len++] = (next_offset >> 7) & 0x7f;
            }
            prev_action_offset = action_offset;
            first_action_offset = action_offset;
        }
        free(handler_types);
        action_idx = first_action_offset + 1; /* 1-based index into action table */
    }
    
    gcc_gen_be_eh_region_t *r = (gcc_gen_be_eh_region_t *)malloc(sizeof(gcc_gen_be_eh_region_t));
    r->try_id = try_id;
    r->landing_pad_id = lp_id;
    r->action_table_index = action_idx;
    r->next = eh_state.regions;
    eh_state.regions = r;

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_lvalue *frame_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "caught_exc");
    gcc_jit_lvalue *frame_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "caught_tinfo");
    
    void *cached_begin = NULL;
    cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_begin_catch", &cached_begin);
    gcc_jit_function *begin_catch_fn = (gcc_jit_function *)cached_begin;
    if (!begin_catch_fn) return GCC_GEN_BE_ERROR_INVALID_STATE;
    
    void *cached_end = NULL;
    cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_end_catch", &cached_end);
    gcc_jit_function *end_catch_fn = (gcc_jit_function *)cached_end;
    if (!end_catch_fn) return GCC_GEN_BE_ERROR_INVALID_STATE;
    
    gcc_jit_function *cxa_type_match = NULL;
    GCC_GEN_BE_CHECK(declare_cxa_type_match(ctx, &cxa_type_match));
    
    gcc_jit_rvalue *exc_val = gcc_jit_lvalue_as_rvalue(frame_exc);
    gcc_jit_rvalue *tinfo_val = gcc_jit_lvalue_as_rvalue(frame_tinfo);
    
    gcc_jit_rvalue *begin_catch_call = gcc_jit_context_new_call(ctx, NULL, begin_catch_fn, 1, &exc_val);
    gcc_jit_lvalue *adj_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "adj_exc");
    gcc_jit_block_add_assignment(catch_dispatch_block, NULL, adj_exc, begin_catch_call);
    
    gcc_jit_block *curr_dispatch = catch_dispatch_block;
    a_handler_ptr handler = tsp->handlers;
    
    gcc_jit_block *rethrow_block = gcc_jit_function_new_block(func, "rethrow_unhandled");
    
    while (handler) {
        gcc_jit_block *match_block = gcc_jit_function_new_block(func, "catch_match");
        gcc_jit_block *next_dispatch = gcc_jit_function_new_block(func, "catch_next");
        
        if (!handler->parameter) {
            /* Catch-all handler */
            gcc_jit_block_end_with_jump(curr_dispatch, NULL, match_block);
        } else {
            /* Polymorphic catch matching */
            gcc_jit_rvalue *catch_tinfo = NULL;
            if (handler->typeinfo_var) {
                gcc_jit_lvalue *tinfo_lval = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_variable(handler->typeinfo_var, &tinfo_lval));
                catch_tinfo = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(tinfo_lval, NULL), void_ptr_type);
            } else {
                catch_tinfo = gcc_jit_context_null(ctx, void_ptr_type);
            }
            
            gcc_jit_rvalue *args[3] = { catch_tinfo, tinfo_val, gcc_jit_lvalue_as_rvalue(adj_exc) };
            gcc_jit_rvalue *match_res = gcc_jit_context_new_call(ctx, NULL, cxa_type_match, 3, args);
            gcc_jit_lvalue *match_ptr = gcc_jit_function_new_local(func, NULL, void_ptr_type, "match_ptr");
            gcc_jit_block_add_assignment(curr_dispatch, NULL, match_ptr, match_res);
            
            gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
            gcc_jit_rvalue *is_match = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(match_ptr), null_ptr);
            gcc_jit_block_end_with_conditional(curr_dispatch, NULL, is_match, match_block, next_dispatch);
            
            /* If match, assign adj_exc to matched pointer */
            gcc_jit_block_add_assignment(match_block, NULL, adj_exc, gcc_jit_lvalue_as_rvalue(match_ptr));
        }
        
        /* Inside match_block */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(match_block));
        
        if (handler->parameter) {
            gcc_jit_lvalue *param_lval = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_variable(handler->parameter, &param_lval));
            
            a_type_ptr param_type = handler->parameter->type;
            if (is_reference_type(param_type)) {
                /* Catch by reference */
                gcc_jit_type *ptype = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(param_type, &ptype));
                gcc_jit_block_add_assignment(match_block, NULL, param_lval, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(adj_exc), ptype));
            } else {
                                /* Catch by value */
                if (handler->dynamic_init && handler->dynamic_init->kind == dik_expression) {
                    gcc_jit_rvalue *init_val = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(handler->dynamic_init->variant.expression, &init_val));
                    if (init_val) {
                        gcc_jit_block_add_assignment(match_block, NULL, param_lval, init_val);
                    }
                }
            }
        }
        
        if (handler->statement) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(handler->statement, func));
        }
        
        gcc_jit_block *after_handler = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_handler));
        if (after_handler) {
            gcc_jit_block_add_eval(after_handler, NULL, gcc_jit_context_new_call(ctx, NULL, end_catch_fn, 0, NULL));
            gcc_jit_block_end_with_jump(after_handler, NULL, end_try_block);
        }
        
        curr_dispatch = next_dispatch;
        handler = handler->next;
        if (!handler) {
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(curr_dispatch));
            gcc_jit_block_end_with_jump(curr_dispatch, NULL, rethrow_block);
        }
    }
    
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rethrow_block));
    /* Unhandled exception: Resume unwinding via _Unwind_Resume */
    void *cached_resume = NULL;
    cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_Unwind_Resume", &cached_resume);
    if (!cached_resume) {
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_param *resume_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "exc");
        gcc_jit_function *resume_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_Unwind_Resume", 1, &resume_param, 0);
        cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"_Unwind_Resume", resume_fn);
        cached_resume = resume_fn;
    }
    if (cached_resume) {
        gcc_jit_block_add_eval(rethrow_block, NULL, gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_resume, 1, &exc_val));
    }
    gcc_jit_block_end_with_jump(rethrow_block, NULL, end_try_block);
    
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_try_block));
    
    return GCC_GEN_BE_SUCCESS;

#endif /* BACK_END_IS_GCC_GEN_BE */

gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    
    a_boolean needs_enforcement = FALSE;
    a_boolean is_noexcept = FALSE;
    
    if (exc_spec) {
        if (exc_spec->is_noexcept && !exc_spec->throw_any) {
            needs_enforcement = TRUE;
            is_noexcept = TRUE;
        } else if (!exc_spec->throw_any) {
            /* Dynamic exception specification throw(A, B, ...) */
            needs_enforcement = TRUE;
            is_noexcept = FALSE;
        }
    }
    
    if (!needs_enforcement) {
        return gcc_gen_be_lower_statement(stmt, func);
    }
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    
    int try_id = eh_state.next_try_id++;
    int lp_id = try_id; /* for simplicity */
    
    if (!eh_state.lp_start_emitted) {
        char buf[128];
        snprintf(buf, sizeof(buf), ".Llp_start_%d:
", eh_state.current_func_id);
        gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(cblock, NULL, buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
        eh_state.lp_start_emitted = TRUE;
    }
    
    char try_start_buf[128];
    snprintf(try_start_buf, sizeof(try_start_buf), ".Ltry_start_%d_%d:
", eh_state.current_func_id, try_id);
    gcc_jit_extended_asm *asm_start = gcc_jit_block_add_extended_asm(cblock, NULL, try_start_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_start, 1);

    gcc_jit_block *try_body_block = gcc_jit_function_new_block(func, "try_body");
    gcc_jit_block *catch_dispatch_block = gcc_jit_function_new_block(func, "catch_dispatch");
    gcc_jit_block *end_try_block = gcc_jit_function_new_block(func, "end_try");

    #ifdef GCC_GEN_BE_USE_SJLJ_EXCEPTIONS
    /* Architecture SjLj Exception Handling Fallback */
    gcc_jit_type *eh_type = NULL;
    void *cached_eh_type = NULL;
    cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_type);
    if (cached_eh_type) {
        eh_type = (gcc_jit_type *)cached_eh_type;
        gcc_jit_lvalue *sjlj_frame = gcc_jit_function_new_local(func, NULL, eh_type, "sjlj_frame");
        gcc_jit_lvalue *chain = NULL;
        get_eh_chain(ctx, &chain);
        
        /* sjlj_frame.next = *chain */
        gcc_jit_field *f_next = gcc_jit_type_get_field(eh_type, 0); /* next is index 0 */
        gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next);
        gcc_jit_block_add_assignment(cblock, NULL, sjlj_next, gcc_jit_lvalue_as_rvalue(chain));
        
        /* *chain = &sjlj_frame */
        gcc_jit_block_add_assignment(cblock, NULL, chain, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(sjlj_frame, NULL), void_ptr_type));
        
        /* Call setjmp */
        void *cached_setjmp = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp);
        if (!cached_setjmp) {
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn);
            cached_setjmp = setjmp_fn;
        }
        
        gcc_jit_field *f_jmpbuf = gcc_jit_type_get_field(eh_type, 1); /* jmpbuf is index 1 */
        gcc_jit_rvalue *jmpbuf_addr = gcc_jit_lvalue_get_address(gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_jmpbuf), NULL);
        gcc_jit_rvalue *setjmp_call = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_addr);
        
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
        gcc_jit_rvalue *is_zero = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, setjmp_call, zero);
        
        gcc_jit_block_end_with_conditional(cblock, NULL, is_zero, try_body_block, catch_dispatch_block);
    } else {
        gcc_jit_block_end_with_jump(cblock, NULL, try_body_block);
    }
#else
    gcc_jit_block_end_with_jump(cblock, NULL, try_body_block);
#endif

    /* Normal execution */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(try_body_block));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(stmt, func));

    gcc_jit_block *after_try = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_try));
    if (after_try) {
#ifdef GCC_GEN_BE_USE_SJLJ_EXCEPTIONS
        void *cached_eh_type = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_type);
        if (cached_eh_type) {
            gcc_jit_type *eh_type = (gcc_jit_type *)cached_eh_type;
            gcc_jit_lvalue *chain = NULL;
            get_eh_chain(ctx, &chain);
            gcc_jit_rvalue *chain_val = gcc_jit_lvalue_as_rvalue(chain);
            gcc_jit_rvalue *chain_deref = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, chain_val, gcc_jit_type_get_pointer(eh_type)), NULL));
            gcc_jit_rvalue *next_val = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, chain_val, gcc_jit_type_get_pointer(eh_type)), NULL), NULL, gcc_jit_type_get_field(eh_type, 0)));
            gcc_jit_block_add_assignment(after_try, NULL, chain, next_val);
        }
#endif
        char try_end_buf[128];
        snprintf(try_end_buf, sizeof(try_end_buf), ".Ltry_end_%d_%d:
", eh_state.current_func_id, try_id);
        gcc_jit_extended_asm *asm_end = gcc_jit_block_add_extended_asm(after_try, NULL, try_end_buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_end, 1);
        gcc_jit_block_end_with_jump(after_try, NULL, end_try_block);
    }

    /* Exception caught (Landing Pad) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_dispatch_block));
    
    char lp_buf[128];
    snprintf(lp_buf, sizeof(lp_buf), ".Llanding_pad_%d_%d:
", eh_state.current_func_id, lp_id);
    gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(catch_dispatch_block, NULL, lp_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
    
    int action_idx = 0;
    if (is_noexcept) {
        /* Catch-all for noexcept */
        int action_offset = eh_state.action_table_len;
        eh_state.action_table[eh_state.action_table_len++] = 0; /* filter = 0 */
        eh_state.action_table[eh_state.action_table_len++] = 0; /* next = 0 */
        action_idx = action_offset + 1;
    } else {
        /* Dynamic exception specification. Encode as catch-all for now to simplify, 
           since our landing pad manually checks and rethrows or calls unexpected. */
        int action_offset = eh_state.action_table_len;
        eh_state.action_table[eh_state.action_table_len++] = 0; /* filter = 0 */
        eh_state.action_table[eh_state.action_table_len++] = 0; /* next = 0 */
        action_idx = action_offset + 1;
    }
    
    gcc_gen_be_eh_region_t *r = (gcc_gen_be_eh_region_t *)malloc(sizeof(gcc_gen_be_eh_region_t));
    r->try_id = try_id;
    r->landing_pad_id = lp_id;
    r->action_table_index = action_idx;
    r->next = eh_state.regions;
    eh_state.regions = r;
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_lvalue *frame_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "caught_exc");
    gcc_jit_lvalue *frame_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "caught_tinfo");
    
    gcc_jit_rvalue *exc_val = gcc_jit_lvalue_as_rvalue(frame_exc);
    gcc_jit_rvalue *tinfo_val = gcc_jit_lvalue_as_rvalue(frame_tinfo);
    
    if (is_noexcept) {
        void *cached_term = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_ZSt9terminatev", &cached_term);
        if (cached_term) {
            gcc_jit_block_add_eval(catch_dispatch_block, NULL, gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_term, 0, NULL));
        }
        gcc_jit_block_end_with_jump(catch_dispatch_block, NULL, end_try_block);
    } else {
        /* Dynamic exception specification */
        gcc_jit_block *unexp_block = gcc_jit_function_new_block(func, "unexpected_block");
        gcc_jit_block *curr_dispatch = catch_dispatch_block;
        
        gcc_jit_function *cxa_type_match = NULL;
        GCC_GEN_BE_CHECK(declare_cxa_type_match(ctx, &cxa_type_match));
        
        void *cached_begin = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_begin_catch", &cached_begin);
        gcc_jit_function *begin_catch_fn = (gcc_jit_function *)cached_begin;
        
        void *cached_end = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_end_catch", &cached_end);
        gcc_jit_function *end_catch_fn = (gcc_jit_function *)cached_end;
        
        an_exception_specification_type_ptr estp = exc_spec->exception_specification_type_list;
        while (estp) {
            if (!estp->redundant) {
                gcc_jit_block *match_block = gcc_jit_function_new_block(func, "catch_match");
                gcc_jit_block *next_dispatch = gcc_jit_function_new_block(func, "catch_next");
                
                gcc_jit_lvalue *tinfo_lval = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(estp->type, &tinfo_lval));
                gcc_jit_rvalue *catch_tinfo = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(tinfo_lval, NULL), void_ptr_type);
                
                gcc_jit_rvalue *args[3] = { catch_tinfo, tinfo_val, exc_val };
                gcc_jit_rvalue *match_res = gcc_jit_context_new_call(ctx, NULL, cxa_type_match, 3, args);
                gcc_jit_lvalue *match_ptr = gcc_jit_function_new_local(func, NULL, void_ptr_type, "match_ptr");
                gcc_jit_block_add_assignment(curr_dispatch, NULL, match_ptr, match_res);
                
                gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
                gcc_jit_rvalue *is_match = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(match_ptr), null_ptr);
                gcc_jit_block_end_with_conditional(curr_dispatch, NULL, is_match, match_block, next_dispatch);
                
                /* If matched, the exception is allowed. We just rethrow it or let it propagate.
                   Actually, if it matches, it's allowed to propagate out of the function.
                   To propagate out, we just rethrow to the NEXT handler up the chain. */
                void *cached_rethrow = NULL;
                cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_rethrow", &cached_rethrow);
                if (cached_rethrow) {
                    gcc_jit_block_add_eval(match_block, NULL, gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_rethrow, 0, NULL));
                }
                gcc_jit_block_end_with_jump(match_block, NULL, end_try_block);
                
                curr_dispatch = next_dispatch;
            }
            estp = estp->next;
        }
        
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(curr_dispatch));
        gcc_jit_block_end_with_jump(curr_dispatch, NULL, unexp_block);
        
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(unexp_block));
        void *cached_unexp = NULL;
        cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_call_unexpected", &cached_unexp);
        if (cached_unexp) {
            gcc_jit_block_add_eval(unexp_block, NULL, gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_unexp, 1, &exc_val));
        }
        gcc_jit_block_end_with_jump(unexp_block, NULL, end_try_block);
    }
    
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_try_block));
    return GCC_GEN_BE_SUCCESS;
}
}

#endif /* BACK_END_IS_GCC_GEN_BE */
