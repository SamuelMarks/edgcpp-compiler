/**
 * @file gcc_gen_be_stmt.c
 * @brief Implementation of statement lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_rtti.h"
#include "func_def.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_type.h"
#include "expr.h"
#include "il_def.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




/**
 * @brief Gets or creates a guard variable for thread-safe initialization.
 *
 * @details Looks up or creates a unique guard variable for the given static local.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] var The frontend variable representing the static local.
 * @param[out] out_guard Pointer to receive the lvalue of the guard.
 * @return GCC_GEN_BE_SUCCESS on success.
 */

static a_label_ptr *computed_goto_labels = NULL;
/** @brief Number of collected computed goto labels. */
static int computed_goto_labels_count = 0;
/** @brief Capacity of the collected computed goto labels array. */
static int computed_goto_labels_capacity = 0;

/** @brief The basic block for dispatching computed gotos. */
static struct gcc_jit_block *computed_goto_dispatcher_block = NULL;
/** @brief The local variable storing the target address for computed gotos. */
static struct gcc_jit_lvalue *computed_goto_target_lval = NULL;

/**
 * @brief Begins tracking statement generation for a function.
 * @details Resets global state for computed gotos.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_begin_function_stmts(void) GCC_GEN_BE_NOEXCEPT {
    computed_goto_labels_count = 0;
    computed_goto_dispatcher_block = NULL;
    computed_goto_target_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Ends tracking statement generation for a function.
 * @details Synthesizes the computed goto dispatcher block if needed.
 * @param[in] func The function being finalized.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_end_function_stmts(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!computed_goto_dispatcher_block) {
        computed_goto_labels_count = 0;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_get_context(&ctx);
    
    gcc_jit_block *default_block = gcc_jit_function_new_block(func, "computed_goto_default");
    gcc_jit_function *trap_func = gcc_jit_context_get_builtin_function(ctx, "__builtin_trap");
    if (trap_func) {
        gcc_jit_block_add_eval(default_block, NULL, gcc_jit_context_new_call(ctx, NULL, trap_func, 0, NULL));
    }
    gcc_jit_block_end_with_void_return(default_block, NULL); // Or jump to self
    
    gcc_jit_case **cases = NULL;
    if (computed_goto_labels_count > 0) {
        cases = malloc(computed_goto_labels_count * sizeof(gcc_jit_case*));
        if (!cases) return GCC_GEN_BE_ERROR_OOM;
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        for (int i = 0; i < computed_goto_labels_count; ++i) {
            a_label_ptr lbl = computed_goto_labels[i];
            gcc_jit_block *lbl_block = NULL;
            gcc_gen_be_get_label_block(func, lbl, &lbl_block);
            gcc_jit_rvalue *val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(uintptr_t)lbl);
            cases[i] = gcc_jit_context_new_case(ctx, val, val, lbl_block);
        }
    }
    
    gcc_jit_rvalue *target_val = gcc_jit_lvalue_as_rvalue(computed_goto_target_lval);
    gcc_jit_rvalue *long_target = gcc_jit_context_new_cast(ctx, NULL, target_val, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG));
    
    gcc_jit_block_end_with_switch(computed_goto_dispatcher_block, NULL, long_target, default_block, computed_goto_labels_count, cases);
    
    if (cases) free(cases);
    computed_goto_dispatcher_block = NULL;
    computed_goto_target_lval = NULL;
    computed_goto_labels_count = 0;
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Gets or creates a guard variable for thread-safe initialization.
 * @details Looks up or creates a unique guard variable for the given static local.
 * @param[in] ctx The libgccjit context.
 * @param[in] var The frontend variable representing the static local.
 * @param[out] out_guard Pointer to receive the lvalue of the guard.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_get_or_create_guard(gcc_jit_context *ctx, a_variable_ptr var, gcc_jit_lvalue **out_guard) {
    if (!out_guard) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_guard = NULL;

    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_GUARD, var, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (cached) {
        *out_guard = (gcc_jit_lvalue *)cached;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *long_long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    
    char guard_name[128];
    snprintf(guard_name, sizeof(guard_name), "_ZGV_local_guard_%p", (void *)var);

    struct gcc_jit_lvalue *guard = gcc_jit_context_new_global(
        ctx,
        NULL,
        GCC_JIT_GLOBAL_INTERNAL,
        long_long_type,
        guard_name
    );

    err = cache_insert(GCC_GEN_BE_CACHE_GUARD, var, guard);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    *out_guard = guard;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves a __cxa_guard function.
 *
 * @details Finds or creates the declaration for the requested guard function.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] name The name of the function to retrieve.
 * @param[in] return_type The expected return type.
 * @param[out] out_func Pointer to receive the function pointer.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t get_cxa_guard_func(gcc_jit_context *ctx, const char *name, gcc_jit_type *return_type, gcc_jit_function **out_func) {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)name, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (cached) {
        *out_func = (gcc_jit_function *)cached;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *guard_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG));
    gcc_jit_param *param = gcc_jit_context_new_param(ctx, NULL, guard_ptr_type, "g");
    
    gcc_jit_function *func = gcc_jit_context_new_function(
        ctx,
        NULL,
        GCC_JIT_FUNCTION_IMPORTED,
        return_type,
        name,
        1,
        &param,
        0
    );

    err = cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)name, func);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    *out_func = func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a declaration statement.
 *
 * @details Processes a local variable declaration and its initializer.
 *
 * @param[in] stmt The frontend statement.
 * @param[in] func The containing gcc_jit_function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_statement_decl(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_gen_be_error_t err;
    err = GCC_GEN_BE_SUCCESS;
    
    an_il_entity_list_entry_ptr ent = stmt->variant.decl.entities;
    while (ent) {
        if (ent->entity.kind == iek_variable) {
            a_variable_ptr var = (a_variable_ptr)ent->entity.ptr;
            /* Only allocate block-scoped automatic variables here.
               Globals, statics, and parameters are handled elsewhere. */
            if (!var->is_parameter && var->storage_class != sc_static && 
                var->storage_class != sc_extern && !var->is_thread_local) {

                gcc_jit_type *vtype = NULL;
                err = gcc_gen_be_lower_type(var->type, &vtype);
                if (err != GCC_GEN_BE_SUCCESS) return err;

                const char *lname = var->source_corresp.name ? var->source_corresp.name : "unnamed_local";
                
                gcc_jit_location *loc = NULL;
                
                gcc_gen_be_get_location_from_var(var, &loc);
                
                struct gcc_jit_lvalue *local = gcc_jit_function_new_local(func, loc, vtype, lname);
                
                /**
 * @brief Emits variable debug info.
 * @details Stub for variable.
 * @param[in] var Variable.
 * @param[out] lval Lvalue.
 * @return GCC_GEN_BE_SUCCESS
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
                gcc_gen_be_emit_variable_debug_info(var, local);
                
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, local));
            }
        }
        ent = ent->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_label_block(struct gcc_jit_function *func, a_label_ptr label, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!label) return GCC_GEN_BE_SUCCESS;
    
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_LABEL, label, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    if (!cached) {
        const char *name = "label";
        if (label->source_corresp.name) {
            name = label->source_corresp.name;
        } else if (label->break_label) {
            name = "break";
        } else if (label->continue_label) {
            name = "continue";
        }
        gcc_jit_block *block = gcc_jit_function_new_block(func, name);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, label, block));
        if (computed_goto_labels_count == computed_goto_labels_capacity) {
            computed_goto_labels_capacity = computed_goto_labels_capacity ? computed_goto_labels_capacity * 2 : 16;
            computed_goto_labels = realloc(computed_goto_labels, computed_goto_labels_capacity * sizeof(a_label_ptr));
        }
        computed_goto_labels[computed_goto_labels_count++] = label;

        *out_block = block;
    } else {
        *out_block = (gcc_jit_block *)cached;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_switch_case_block(struct gcc_jit_function *func, a_switch_case_entry_ptr scep, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!scep) return GCC_GEN_BE_SUCCESS;
    
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_LABEL, scep, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    if (!cached) {
        gcc_jit_block *block = gcc_jit_function_new_block(func, scep->case_value ? "case" : "default");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, scep, block));
        *out_block = block;
    } else {
        *out_block = (gcc_jit_block *)cached;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  gcc_gen_be_error_t err;

  while (stmt) {
      gcc_jit_block *current_block = NULL;
      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
      
      if (!current_block) {
          stmt = stmt->next;
          continue;
      }
      
      gcc_jit_location *loc = NULL;
      
      gcc_gen_be_get_location_from_stmt(stmt, &loc);
      if (loc && current_block) {
          gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
          /* In a production DWARF mapping, we'd emit DW_TAG_lexical_block via specialized API, 
             or tag operations. For libgccjit, we add a dummy eval to establish the line number. */
          gcc_jit_block_add_eval(current_block, loc, gcc_jit_context_null(ctx, void_ptr_type));
      }
          if (stmt->kind == stmk_label) {
              gcc_jit_block *lbl_block = NULL;
              err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &lbl_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(lbl_block));
              current_block = lbl_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = NULL;
              err = gcc_gen_be_get_switch_case_block(func, stmt->variant.switch_case.extra_info, &case_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(case_block));
              current_block = case_block;
          } else {
              stmt = stmt->next;
              continue;
          }
      } else {
          if (stmt->kind == stmk_label) {
              gcc_jit_block *label_block = NULL;
              err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &label_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block_end_with_jump(current_block, loc, label_block);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(label_block));
              current_block = label_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = NULL;
              err = gcc_gen_be_get_switch_case_block(func, stmt->variant.switch_case.extra_info, &case_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block_end_with_jump(current_block, loc, case_block);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(case_block));
              current_block = case_block;
          }
      }

      switch (stmt->kind) {
          case stmk_block:
              {
                  an_object_lifetime_ptr lifetime = NULL;
                  if (stmt->variant.block.extra_info) {
                      if (stmt->variant.block.extra_info->assoc_scope) {
                          lifetime = stmt->variant.block.extra_info->assoc_scope->lifetime;
                      } else {
                          lifetime = stmt->variant.block.extra_info->lifetime;
                      }
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(lifetime));
                  if (stmt->variant.block.statements) {
                      err = gcc_gen_be_lower_statement(stmt->variant.block.statements, func);
                      if (err != GCC_GEN_BE_SUCCESS) {
                          (void)gcc_gen_be_pop_scope(NULL);
                          return err;
                      }
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(current_block));
              }
              break;

          case stmk_expr:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (rval && current_block) gcc_jit_block_add_eval(current_block, loc, rval);
              }
              break;

          case stmk_stmt_expr_result:
              {
                  extern struct gcc_jit_lvalue *current_stmt_expr_result_lval;
                  if (current_stmt_expr_result_lval) {
                      gcc_jit_rvalue *res_val = NULL;
                      if (stmt->expr) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->expr, &res_val));
                      } else if (stmt->variant.stmt_expr_result.dynamic_init) {
                          a_dynamic_init_ptr dip = stmt->variant.stmt_expr_result.dynamic_init;
                          if (dip->kind == dik_expression || dip->kind == dik_class_result_via_ctor) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &res_val));
                          }
                          /* We ignore dik_constructor for now since it is not fully supported */
                      }
                      
                      if (res_val) {
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_block_add_assignment(cblock, loc, current_stmt_expr_result_lval, res_val);
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_coroutine:
              {
                  /* Coroutine frame allocation and dispatcher synthesis */
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  /* Layout coroutine heap frame: resume fn, destroy fn, promise, state index, etc. */
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                  
                  gcc_jit_param *resume_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame_ptr");
                  gcc_jit_type *resume_func_type = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_type, 1, &void_ptr_type, 0);
                  
                  gcc_jit_field *f_resume = gcc_jit_context_new_field(ctx, NULL, resume_func_type, "resume_fn");
                  gcc_jit_field *f_destroy = gcc_jit_context_new_field(ctx, NULL, resume_func_type, "destroy_fn");
                  gcc_jit_field *f_promise = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "promise_obj"); /* placeholder */
                  gcc_jit_field *f_state = gcc_jit_context_new_field(ctx, NULL, int_type, "state_index");
                  gcc_jit_field *f_locals = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "saved_locals");
                  
                  gcc_jit_field *frame_fields[] = { f_resume, f_destroy, f_promise, f_state, f_locals };
                  gcc_jit_struct *frame_struct = gcc_jit_context_new_struct_type(ctx, NULL, "coroutine_frame", 5, frame_fields);
                  gcc_jit_type *frame_struct_type = gcc_jit_struct_as_type(frame_struct);
                  gcc_jit_type *frame_struct_ptr_type = gcc_jit_type_get_pointer(frame_struct_type);
                  
                  /* Allocate frame using operator new or get_return_object_on_allocation_failure */
                  gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                  gcc_jit_param *malloc_param = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
                  gcc_jit_function *malloc_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__builtin_alloca", 1, &malloc_param, 0);
                  
                  /* Ramp function setup: evaluate size, allocate */
                  gcc_jit_rvalue *frame_size = gcc_jit_context_new_sizeof(ctx, frame_struct_type);
                  gcc_jit_rvalue *frame_ptr = gcc_jit_context_new_call(ctx, NULL, malloc_func, 1, &frame_size);
                  
                  gcc_jit_lvalue *coro_frame = gcc_jit_function_new_local(func, NULL, frame_struct_ptr_type, "coro_frame");
                  gcc_jit_rvalue *cast_frame_ptr = gcc_jit_context_new_cast(ctx, NULL, frame_ptr, frame_struct_ptr_type);
                  gcc_jit_block_add_assignment(cblock, loc, coro_frame, cast_frame_ptr);
                  
                  /* Construct promise object and save to frame */
                  /* In a real implementation we evaluate stmt->variant.coroutine.descr variables */
                  gcc_jit_rvalue *null_promise = gcc_jit_context_null(ctx, void_ptr_type);
                  gcc_jit_lvalue *promise_field = gcc_jit_lvalue_access_field(gcc_jit_lvalue_dereference(coro_frame, NULL), NULL, f_promise);
                  gcc_jit_block_add_assignment(cblock, loc, promise_field, null_promise);
                  
                  /* Synthesize resume function dispatcher: switch on state index */
                  gcc_jit_block *coro_resume = gcc_jit_function_new_block(func, "coro_resume_dispatcher");
                  gcc_jit_block *coro_destroy = gcc_jit_function_new_block(func, "coro_destroy_dispatcher");
                  
                  gcc_jit_lvalue *state_field = gcc_jit_lvalue_access_field(gcc_jit_lvalue_dereference(coro_frame, NULL), NULL, f_state);
                  
                  /* Dummy switch statement for resumption dispatcher */
                  gcc_jit_rvalue *state_val = gcc_jit_lvalue_as_rvalue(state_field);
                  gcc_jit_block *state_0_block = gcc_jit_function_new_block(func, "coro_resume_state_0");
                  gcc_jit_case *case_0 = gcc_jit_context_new_case(ctx, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0), gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0), state_0_block);
                  gcc_jit_block_end_with_switch(coro_resume, NULL, state_val, coro_resume, 1, &case_0);
                  
                  /* Run destructors for promise and captured variables in frame, then operator delete */
                  gcc_jit_param *free_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
                  gcc_jit_function *free_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "free", 1, &free_param, 0);
                  gcc_jit_rvalue *void_frame_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(coro_frame), void_ptr_type);
                  gcc_jit_block_add_eval(coro_destroy, NULL, gcc_jit_context_new_call(ctx, NULL, free_func, 1, &void_frame_ptr));
                  
                  /* Execute initial suspend (co_await promise.initial_suspend()) */
                  /* Handled by EDG lowered IL or we lower descr->initial_suspend_call */
                  if (stmt->variant.coroutine.descr && stmt->variant.coroutine.descr->initial_suspend_call) {
                      gcc_jit_rvalue *init_susp_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->variant.coroutine.descr->initial_suspend_call, &init_susp_rval));
                      if (init_susp_rval) {
                          gcc_jit_block_add_eval(cblock, loc, init_susp_rval);
                      }
                  }
                  
                  gcc_jit_block_end_with_jump(state_0_block, NULL, state_0_block);
                  gcc_jit_block_end_with_void_return(coro_destroy, NULL);
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_coroutine_return:
              {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_block_end_with_void_return(cblock, loc);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_return:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (!rval) rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  if (current_block) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, current_block));
                      gcc_jit_block_end_with_return(current_block, NULL, rval);
                  }
              } else {
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (current_block) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, current_block));
                      gcc_jit_block_end_with_void_return(current_block, NULL);
                  }
              }
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              break;

          case stmk_goto:
              {
                  gcc_jit_block *target = NULL;
                  err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &target);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (current_block) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(stmt->variant.label.lifetime, current_block));
                      gcc_jit_block_end_with_jump(current_block, loc, target);
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              }
              break;

#if GNU_EXTENSIONS_ALLOWED
          case stmk_assigned_goto:
              {
                  gcc_jit_rvalue *ptr = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->variant.assigned_goto.ptr, &ptr));
                  
                  if (!computed_goto_dispatcher_block) {
                      computed_goto_dispatcher_block = gcc_jit_function_new_block(func, "computed_goto_dispatcher");
                      computed_goto_target_lval = gcc_jit_function_new_local(func, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "computed_goto_target");
                  }
                  
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_rvalue *ptr_void = gcc_jit_context_new_cast(ctx, NULL, ptr, void_ptr_type);
                  gcc_jit_block_add_assignment(current_block, loc, computed_goto_target_lval, ptr_void);
                  gcc_jit_block_end_with_jump(current_block, loc, computed_goto_dispatcher_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              }
              break;
#endif

          case stmk_if:
          case stmk_constexpr_if:
          case stmk_if_consteval:
          case stmk_if_not_consteval:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  cond = gcc_jit_context_new_cast(ctx, NULL, cond, bool_type);

                  gcc_jit_block *then_block = gcc_jit_function_new_block(func, "if_then");
                  gcc_jit_block *else_block = gcc_jit_function_new_block(func, "if_else");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "if_merge");

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  gcc_jit_block_end_with_conditional(current_block, loc, cond, then_block, else_block);

                  gcc_jit_block *after_then = NULL;
                  gcc_jit_block *after_else = NULL;

                  if (stmt->kind == stmk_constexpr_if) {
                      if (stmt->variant.constexpr_if->then_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.constexpr_if->then_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_then));
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.constexpr_if->else_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.constexpr_if->else_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_else));
                      }
                      else
                          after_else = else_block;
                  } else {
                      if (stmt->variant.if_stmt.then_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.if_stmt.then_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_then));
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.if_stmt.else_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.if_stmt.else_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_else));
                      }
                      else
                          after_else = else_block;
                  }

                  if (after_then) gcc_jit_block_end_with_jump(after_then, NULL, merge_block);
                  if (after_else) gcc_jit_block_end_with_jump(after_else, NULL, merge_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
              }
              break;

          case stmk_switch:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));

                  a_switch_stmt_descr_ptr ssdp = stmt->variant.switch_stmt.extra_info;
                  int num_cases = 0;
                  a_switch_case_entry_ptr scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) num_cases++;
                      scep = scep->next;
                  }

                  gcc_jit_case **cases = NULL;
                  if (num_cases > 0) {
                      cases = (gcc_jit_case **)malloc((size_t)num_cases * sizeof(gcc_jit_case *));
                      if (!cases) return GCC_GEN_BE_ERROR_OOM;
                  }

                  int i = 0;
                  scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) {
                          long val_int = 0;
                          if (scep->case_value && scep->case_value->kind == ck_integer) {
                              a_boolean local_err = FALSE;
                              val_int = (long)value_of_integer_constant(scep->case_value, &local_err);
                          }
                          gcc_jit_rvalue *val = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), val_int);
                          val = gcc_jit_context_new_cast(ctx, NULL, val, gcc_jit_rvalue_get_type(cond));
                          
                          gcc_jit_rvalue *max_val = val;
#if GNU_EXTENSIONS_ALLOWED
                          if (scep->range_end && scep->range_end->kind == ck_integer) {
                              a_boolean local_err = FALSE;
                              long max_int = (long)value_of_integer_constant(scep->range_end, &local_err);
                              max_val = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), max_int);
                              max_val = gcc_jit_context_new_cast(ctx, NULL, max_val, gcc_jit_rvalue_get_type(cond));
                          }
#endif
                          
                          gcc_jit_block *case_block = NULL;
                          err = gcc_gen_be_get_switch_case_block(func, scep, &case_block);
                          if (err != GCC_GEN_BE_SUCCESS) { if(cases) free(cases); return err; }
                          
                          cases[i++] = gcc_jit_context_new_case(ctx, val, max_val, case_block);
                      }
                      scep = scep->next;
                  }

                  gcc_jit_block *default_block = NULL;
                  if (ssdp->default_case) {
                      err = gcc_gen_be_get_switch_case_block(func, ssdp->default_case, &default_block);
                      if (err != GCC_GEN_BE_SUCCESS) { if(cases) free(cases); return err; }
                  } else {
                      default_block = gcc_jit_function_new_block(func, "switch_default");
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  gcc_jit_block_end_with_switch(current_block, NULL, cond, default_block, num_cases, cases);
                  if (num_cases > 0) free(cases);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.switch_stmt.body_statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_switch = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_switch));
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "switch_exit");

                  if (!ssdp->default_case) {
                      gcc_jit_block_end_with_jump(default_block, NULL, exit_block);
                  }
                  if (after_switch) {
                      gcc_jit_block_end_with_jump(after_switch, NULL, exit_block);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_while:
          case stmk_end_test_while:
              {
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "loop_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "loop_body");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "loop_exit");

                  if (stmt->kind == stmk_end_test_while) {
                      gcc_jit_block_end_with_jump(current_block, loc, body_block);
                  } else {
                      gcc_jit_block_end_with_jump(current_block, loc, cond_block);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cond_block));
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_one(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  cond = gcc_jit_context_new_cast(ctx, NULL, cond, bool_type);

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cond_block));
                  if (cond_block) gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_block)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.loop_statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_body = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_for:
              {
                  a_for_loop_ptr loop_info = stmt->variant.for_loop.extra_info;
                  if (loop_info && loop_info->initialization) {
                      err = gcc_gen_be_lower_statement(loop_info->initialization, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "for_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "for_body");
                  gcc_jit_block *step_block = gcc_jit_function_new_block(func, "for_step");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "for_exit");

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block)); /* refresh */
                  if (current_block) gcc_jit_block_end_with_jump(current_block, loc, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cond_block));
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_one(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  cond = gcc_jit_context_new_cast(ctx, NULL, cond, bool_type);

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cond_block));
                  if (cond_block) gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_block)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.for_loop.statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_body = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, step_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(step_block));
                  if (loop_info && loop_info->increment) {
                      gcc_jit_rvalue *rval = NULL;
                      err = gcc_gen_be_lower_expr_rvalue(loop_info->increment, &rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&step_block));
                      if (rval && step_block) gcc_jit_block_add_eval(step_block, NULL, rval);
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&step_block));
                  if (step_block) gcc_jit_block_end_with_jump(step_block, NULL, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;


#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_microsoft_try:
              {
                  /* SEH implementation via SjLj fallback */
                  a_microsoft_try_supplement_ptr mtsp = stmt->variant.microsoft_try;
                  if (mtsp->except_expr) {
                      /* __try / __except */
                      gcc_jit_block *guarded_block = gcc_jit_function_new_block(func, "seh_try_body");
                      gcc_jit_block *handler_block = gcc_jit_function_new_block(func, "seh_except_handler");
                      gcc_jit_block *end_block = gcc_jit_function_new_block(func, "seh_end");
                      
                      /* Setup barrier (pseudo setjmp) */
                      /* For fallback, we simply jump to guarded block. */
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      gcc_jit_block_end_with_jump(cblock, loc, guarded_block);
                      
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(guarded_block));
                      if (mtsp->guarded_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->guarded_statement, func));
                      }
                      gcc_jit_block *after_guarded = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_guarded));
                      if (after_guarded) {
                          gcc_jit_block_end_with_jump(after_guarded, NULL, end_block);
                      }
                      
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(handler_block));
                      /* Evaluate except_expr */
                      gcc_jit_rvalue *filter_res = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(mtsp->except_expr, &filter_res));
                      /* We mock the exception execution logic: if 1, execute handler */
                      if (mtsp->cleanup_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->cleanup_statement, func));
                      }
                      gcc_jit_block *after_handler = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_handler));
                      if (after_handler) {
                          gcc_jit_block_end_with_jump(after_handler, NULL, end_block);
                      }
                      
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
                  } else {
                      /* __try / __finally */
                      gcc_jit_block *guarded_block = gcc_jit_function_new_block(func, "seh_finally_try");
                      gcc_jit_block *finally_block = gcc_jit_function_new_block(func, "seh_finally_cleanup");
                      
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      gcc_jit_block_end_with_jump(cblock, loc, guarded_block);
                      
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(guarded_block));
                      if (mtsp->guarded_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->guarded_statement, func));
                      }
                      gcc_jit_block *after_guarded = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_guarded));
                      if (after_guarded) {
                          gcc_jit_block_end_with_jump(after_guarded, NULL, finally_block);
                      }
                      
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(finally_block));
                      if (mtsp->cleanup_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->cleanup_statement, func));
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
              break;
#endif

          case stmk_try_block:
              {
                  err = gcc_gen_be_lower_try_block(stmt, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  return GCC_GEN_BE_SUCCESS;
              }
              break;

          case stmk_range_based_for:
              {
                  if (stmt->variant.range_based_for_loop.statement) {
                      err = gcc_gen_be_lower_statement(stmt->variant.range_based_for_loop.statement, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_for_each:
              {
                  if (stmt->variant.for_each_loop.statement) {
                      err = gcc_gen_be_lower_statement(stmt->variant.for_each_loop.statement, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
#endif



          case stmk_set_vla_size:
              {
                  a_vla_dimension_ptr dim = stmt->variant.vla_dimension;
                  if (dim->dimension_variable != NULL) {
                      gcc_jit_rvalue *ignore = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dim->dimension_expr, &ignore));
                      if (ignore) {
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_block_add_eval(cblock, loc, ignore);
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
              
          case stmk_vla_decl:
              {
                  a_variable_ptr var = stmt->variant.vla.variant.variable;
                  if (!var || stmt->variant.vla.is_typedef_decl) return GCC_GEN_BE_SUCCESS;
                  
                  gcc_jit_type *type = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &type));
                  
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  gcc_jit_lvalue *vla_lval = gcc_jit_function_new_local(func, NULL, type, var->source_corresp.name ? var->source_corresp.name : "vla_var");
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, vla_lval));
                  
                  /* Calculate total size */
                  gcc_jit_type *size_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                  gcc_jit_rvalue *total_elements = gcc_jit_context_new_rvalue_from_long(ctx, size_type, 1);
                  
                  a_type_ptr curr_type = var->type;
                  while (curr_type && curr_type->kind == tk_typeref) curr_type = curr_type->variant.typeref.type;
                  
                  extern struct a_scope *innermost_function_scope;
                  
                  while (curr_type && curr_type->kind == tk_array) {
                      if (curr_type->variant.array.has_assoc_vla_dimension) {
                          a_vla_dimension_ptr dim = innermost_function_scope->vla_dimensions;
                          while (dim) {
                              if (dim->type == curr_type) break;
                              dim = dim->next;
                          }
                          while (dim && !dim->dimension_variable && dim->original_dimension) {
                              dim = dim->original_dimension;
                          }
                          if (dim && dim->dimension_variable) {
                              gcc_jit_lvalue *dim_lval = NULL;
                              extern gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
                              void *cached = NULL;
                              cache_lookup(GCC_GEN_BE_CACHE_VAR, dim->dimension_variable, &cached);
                              if (cached) dim_lval = (gcc_jit_lvalue*)cached;
                              else gcc_gen_be_lower_local_variable_decl(func, dim->dimension_variable, &dim_lval);
                              
                              if (dim_lval) {
                                  gcc_jit_rvalue *dim_val = gcc_jit_lvalue_as_rvalue(dim_lval);
                                  dim_val = gcc_jit_context_new_cast(ctx, NULL, dim_val, size_type);
                                  total_elements = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, size_type, total_elements, dim_val);
                              }
                          }
                      } else if (curr_type->variant.array.variant.number_of_elements > 0) {
                          gcc_jit_rvalue *dim_val = gcc_jit_context_new_rvalue_from_long(ctx, size_type, curr_type->variant.array.variant.number_of_elements);
                          total_elements = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, size_type, total_elements, dim_val);
                      }
                      curr_type = curr_type->variant.array.element_type;
                      while (curr_type && curr_type->kind == tk_typeref) curr_type = curr_type->variant.typeref.type;
                  }
                  
                  gcc_jit_type *elem_type = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(curr_type, &elem_type));
                  
                  /* Workaround: Since __builtin_stack_save crashes libgccjit in some versions, use malloc to allocate VLA */
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_param *malloc_param = gcc_jit_context_new_param(ctx, NULL, size_type, "size");
                  gcc_jit_function *malloc_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__builtin_alloca", 1, &malloc_param, 0);
                  
                  gcc_jit_rvalue *elem_size = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_context_new_sizeof(ctx, elem_type), size_type);
                  gcc_jit_rvalue *total_bytes = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, size_type, total_elements, elem_size);
                  
                  gcc_jit_rvalue *malloc_call = gcc_jit_context_new_call(ctx, NULL, malloc_func, 1, &total_bytes);
                  gcc_jit_rvalue *typed_mem = gcc_jit_context_new_cast(ctx, NULL, malloc_call, type);
                  gcc_jit_block_add_assignment(cblock, loc, vla_lval, typed_mem);
                  
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_decl:
              err = gcc_gen_be_lower_statement_decl(stmt, func);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              break;
              
          case stmk_init:
              {
                  a_dynamic_init_ptr dip = stmt->variant.dynamic_init;
                  if (dip && dip->kind != dik_none && dip->variable) {
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      
                      gcc_jit_lvalue *var_lvalue = (gcc_jit_lvalue *)cached;
                      if (var_lvalue) {
                          gcc_jit_rvalue *rval = NULL;
                          if (dip->kind == dik_zero) {
                              gcc_jit_type *vtype = NULL;
                              err = gcc_gen_be_lower_type(dip->variable->type, &vtype);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              err = gcc_gen_be_get_zero_rvalue(dip->variable->type, vtype, &rval);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                          } else if (dip->kind == dik_constant || dip->kind == dik_nonconstant_aggregate) {
                              a_constant_ptr con = dip->variant.constant.ptr;
                              gcc_jit_type *type = NULL;
                              err = gcc_gen_be_lower_type(dip->variable->type, &type);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              err = gcc_gen_be_lower_constant_rvalue(con, type, &rval);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                          } else if (dip->kind == dik_expression || dip->kind == dik_class_result_via_ctor) {
                              err = gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &rval);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                          }
                          
                          if (rval) {
                              if (dip->variable->storage_class == sc_static || dip->variable->is_thread_local) {
                                  struct gcc_jit_lvalue *guard = NULL;
                                  err = gcc_gen_be_get_or_create_guard(ctx, dip->variable, &guard);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;

                                  gcc_jit_function *acquire_fn = NULL;
                                  err = get_cxa_guard_func(ctx, "__cxa_guard_acquire", gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), &acquire_fn);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;

                                  gcc_jit_function *release_fn = NULL;
                                  err = get_cxa_guard_func(ctx, "__cxa_guard_release", gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), &release_fn);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;

                                  gcc_jit_function *abort_fn = NULL;
                                  err = get_cxa_guard_func(ctx, "__cxa_guard_abort", gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), &abort_fn);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;

                                  gcc_jit_block *init_block = gcc_jit_function_new_block(func, "guard_init");
                                  gcc_jit_block *done_block = gcc_jit_function_new_block(func, "guard_done");

                                  gcc_jit_rvalue *guard_ptr = gcc_jit_lvalue_get_address((gcc_jit_lvalue*)guard, NULL);
                                  gcc_jit_rvalue *acquire_call = gcc_jit_context_new_call(ctx, NULL, acquire_fn, 1, &guard_ptr);
                                  gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, acquire_call, gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT)));

                                  gcc_jit_block_end_with_conditional(current_block, loc, cond, init_block, done_block);

                                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(init_block));
                                  gcc_jit_block_add_assignment(init_block, NULL, (gcc_jit_lvalue*)var_lvalue, rval);
                                  gcc_jit_block_add_eval(init_block, NULL, gcc_jit_context_new_call(ctx, NULL, release_fn, 1, &guard_ptr));
                                  gcc_jit_block_end_with_jump(init_block, NULL, done_block);

                                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(done_block));
                                  current_block = done_block;
                              } else {
                                  gcc_jit_block_add_assignment(current_block, loc, (gcc_jit_lvalue*)var_lvalue, rval);
                                  
                                  if (dip->destructor) {
                                      void *dtor_cached = NULL;
                                      err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, dip->destructor, &dtor_cached);
                                      if (err == GCC_GEN_BE_SUCCESS && dtor_cached) {
                                          GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup((gcc_jit_lvalue*)var_lvalue, (gcc_jit_function *)dtor_cached));
                                      }
                                  }
                              }
                          }
                      }
                  }
              }
              break;

#if ASM_FUNCTION_ALLOWED
          case stmk_asm_func_body:
              {
                  /* Handle naked assembly function bodies.
                     In libgccjit, you can use gcc_jit_context_add_top_level_asm
                     but that's global. For a specific function, if the function is entirely naked ASM,
                     there's no direct `gcc_jit_block_add_asm_func_body`.
                     Extended inline asm works, so we just add it to the block. */
                  if (stmt->variant.asm_func_body) {
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (cblock) {
                          gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(cblock, NULL, stmt->variant.asm_func_body);
                          if (ext_asm) {
                              gcc_jit_extended_asm_set_volatile_flag(ext_asm, 1);
                          }
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
#endif


          case stmk_asm:
              {
                  an_asm_entry_ptr asm_entry = stmt->variant.asm_entry;
                  if (asm_entry) {
                      const char *asm_str = "";
                      if (asm_entry->asm_string && asm_entry->asm_string->kind == ck_string) {
                          asm_str = (const char *)asm_entry->asm_string->variant.string.value;
                      }
                      
#if defined(GCC_JIT_EXTENDED_ASM_GOTO)
                      /* GCC 11+ supports asm goto via gcc_jit_block_end_with_extended_asm_goto */
                      gcc_jit_extended_asm *ext_asm = NULL;
                      if (asm_entry->is_asm_goto) {
                          /* Need to collect blocks from asm_entry->labels */
                          int num_labels = 0;
                          a_label_list_ptr lbl = asm_entry->labels;
                          while (lbl) { num_labels++; lbl = lbl->next; }
                          
                          gcc_jit_block **dest_blocks = NULL;
                          if (num_labels > 0) {
                              dest_blocks = (gcc_jit_block **)malloc((size_t)num_labels * sizeof(gcc_jit_block *));
                              if (!dest_blocks) return GCC_GEN_BE_ERROR_OOM;
                              int i = 0;
                              for (lbl = asm_entry->labels; lbl; lbl = lbl->next) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_label_block(func, lbl->label, &dest_blocks[i++]));
                              }
                          }
                          
                          gcc_jit_block *fallthrough_block = gcc_jit_function_new_block(func, "asm_goto_fallthrough");
                          ext_asm = gcc_jit_block_end_with_extended_asm_goto(current_block, NULL, asm_str, num_labels, dest_blocks, fallthrough_block);
                          
                          if (dest_blocks) free(dest_blocks);
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(fallthrough_block));
                      } else {
                          ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
                      }
#else
                      gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
#endif
                      if (ext_asm) {
                          gcc_jit_extended_asm_set_volatile_flag(ext_asm, asm_entry->is_volatile);
                          
                          an_asm_operand_ptr op = asm_entry->operands;
                          while (op) {
                              const char *name = op->name;
                              const char *constraint = op->constraints_string;
                              if (op->is_output_operand) {
                                  gcc_jit_lvalue *lval = NULL;
                                  err = gcc_gen_be_lower_expr_lvalue(op->expression, &lval);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  if (lval) {
                                      gcc_jit_extended_asm_add_output_operand(ext_asm, name, constraint, (gcc_jit_lvalue*)lval);
                                  }
                              } else {
                                  gcc_jit_rvalue *rval = NULL;
                                  err = gcc_gen_be_lower_expr_rvalue(op->expression, &rval);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  if (rval) {
                                      gcc_jit_extended_asm_add_input_operand(ext_asm, name, constraint, rval);
                                  }
                              }
                              op = op->next;
                          }
                          
                          a_named_register_list_ptr clobber = asm_entry->clobbers;
                          while (clobber) {
                              if (clobber->reg > anr_invalid && clobber->reg < anr_last) {
                                  const char *reg_name = named_register_names[clobber->reg];
                                  if (reg_name) {
                                      gcc_jit_extended_asm_add_clobber(ext_asm, reg_name);
                                  }
                              }
                              clobber = clobber->next;
                          }
                      }
                  }
              }
              break;

          default:
              break;
      }

      stmt = stmt->next;
  }
  
/** @brief GCC_GEN_BE_SUCCESS member. */
  return GCC_GEN_BE_SUCCESS;
}



/**
 * @brief Emits a stack save operation.
 *
 * @details Emits a call to __builtin_stack_save.
 *
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[out] out_save Pointer to receive the saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_stack_save(struct gcc_jit_function *func, struct gcc_jit_block *block, gcc_jit_lvalue **out_save) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !out_save) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    gcc_jit_function *save_fn = p_gcc_jit_context_get_builtin_function ? p_gcc_jit_context_get_builtin_function(ctx, "__builtin_stack_save") : NULL;
    if (save_fn) {
        gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, save_fn, 0, NULL);
        gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
        struct gcc_jit_lvalue *save_var = gcc_jit_function_new_local(func, NULL, void_ptr_type, "saved_stack");
        gcc_jit_block_add_assignment(block, loc, (gcc_jit_lvalue*)save_var, call);
        *out_save = save_var;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a stack restore operation.
 *
 * @details Emits a call to __builtin_stack_restore.
 *
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[in] save_var The saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_stack_restore(struct gcc_jit_function *func, struct gcc_jit_block *block, gcc_jit_lvalue *save_var) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !save_var) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    gcc_jit_function *restore_fn = p_gcc_jit_context_get_builtin_function ? p_gcc_jit_context_get_builtin_function(ctx, "__builtin_stack_restore") : NULL;
    if (restore_fn) {
        gcc_jit_rvalue *arg = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue*)save_var);
        gcc_jit_block_add_eval(block, loc, gcc_jit_context_new_call(ctx, NULL, restore_fn, 1, &arg));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers coroutine frame.
 * @details Stubs coroutine frame.
 * @param[in] rout Routine.
 * @param[out] out_frame_type Frame type.
 * @return GCC_GEN_BE_SUCCESS
 */

/**
 * @brief Collects all local variables in a scope and sub-scopes into a struct fields array.
 * @details Recursively walks EDG frontend scope nodes to build a flat frame layout.
 * @param[in] scope The EDG scope pointer.
 * @param[in] ctx The libgccjit context.
 * @param[in,out] fields Pointer to array of gcc_jit_field pointers.
 * @param[in,out] num_fields Pointer to current number of fields.
 * @param[in,out] capacity Pointer to current capacity of the array.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
static gcc_gen_be_error_t collect_coro_locals(a_scope_ptr scope, gcc_jit_context *ctx, gcc_jit_field ***fields, int *num_fields, int *capacity) {
    if (!scope) return GCC_GEN_BE_SUCCESS;
    
    a_variable_ptr var = scope->nonstatic_variables;
    while (var) {
        if (!var->is_typedef && !var->is_compiler_generated) {
            gcc_jit_type *var_type = NULL;
            gcc_gen_be_error_t err = gcc_gen_be_lower_type(var->type, &var_type);
            if (err == GCC_GEN_BE_SUCCESS && var_type) {
                if (*num_fields >= *capacity) {
                    *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
                    *fields = (gcc_jit_field **)realloc(*fields, *capacity * sizeof(gcc_jit_field *));
                }
                char field_name[128];
                snprintf(field_name, sizeof(field_name), "local_%p", (void *)var);
                (*fields)[*num_fields] = gcc_jit_context_new_field(ctx, NULL, var_type, field_name);
                (*num_fields)++;
            }
        }
        var = var->next;
    }
    
    a_scope_ptr child = scope->scopes;
    while (child) {
        gcc_gen_be_error_t err = collect_coro_locals(child, ctx, fields, num_fields, capacity);
        if (err != GCC_GEN_BE_SUCCESS) return err;
        child = child->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_coroutine_frame(a_routine_ptr rout, struct gcc_jit_type **out_frame_type) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_frame_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    
    gcc_jit_param *fn_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame_ptr");
    gcc_jit_type *dispatcher_type = gcc_jit_type_get_pointer(gcc_jit_function_as_type(
        gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "dummy_dispatcher", 1, &fn_param, 0)));

    gcc_jit_field *f_resume = gcc_jit_context_new_field(ctx, NULL, dispatcher_type, "resume_fn");
    gcc_jit_field *f_destroy = gcc_jit_context_new_field(ctx, NULL, dispatcher_type, "destroy_fn");
    gcc_jit_field *f_promise = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "promise_obj");
    gcc_jit_field *f_state = gcc_jit_context_new_field(ctx, NULL, int_type, "state_index");
    
    int num_fields = 4;
    int capacity = 16;
    gcc_jit_field **fields = (gcc_jit_field **)malloc(capacity * sizeof(gcc_jit_field *));
    fields[0] = f_resume;
    fields[1] = f_destroy;
    fields[2] = f_promise;
    fields[3] = f_state;
    
    if (rout->function_def_number != 0 && il_header.function_def_table) {
        a_scope_ptr func_scope = il_header.function_def_table[rout->function_def_number].scope;
        GCC_GEN_BE_CHECK(collect_coro_locals(func_scope, ctx, &fields, &num_fields, &capacity));
    }
    
    char name[128];
    snprintf(name, sizeof(name), "_Z_coro_frame_%p", (void *)rout);
    gcc_jit_struct *frame_struct = gcc_jit_context_new_struct_type(ctx, NULL, name, num_fields, fields);
    
    free(fields);
    *out_frame_type = gcc_jit_struct_as_type(frame_struct);
    return GCC_GEN_BE_SUCCESS;
}


/**
 * @brief Lowers coroutine ramp.
 * @details Stubs coroutine ramp.
 * @param[in] rout Routine.
 * @param[out] ramp_func Ramp function.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr rout, struct gcc_jit_function *ramp_func) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !ramp_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    gcc_jit_block *entry = gcc_jit_function_new_block(ramp_func, "ramp_entry");
    gcc_jit_block *alloc_done = gcc_jit_function_new_block(ramp_func, "alloc_done");

    a_coroutine_descr_ptr descr = get_coroutine_descr(rout);

    /* Call operator new */
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    
    gcc_jit_type *frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(rout, &frame_type));
    long frame_sz = (long)gcc_jit_type_get_size(frame_type);
    gcc_jit_rvalue *size_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, frame_sz);
    
    gcc_jit_rvalue *frame_ptr = NULL;
    if (descr && descr->new_routine) {
        gcc_jit_function *op_new_fn = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->new_routine, &op_new_fn));
        /* operator new might take more arguments in the case of promise_type::operator new(size, args...)
           but for simplicity, we pass just the size if it's the only one, or stub it out if we don't have full arg expressions here.
           Actually EDG should provide an expression for the allocation if it was complex, but we only have a routine_ptr.
           Let's just call it with size. */
        frame_ptr = gcc_jit_context_new_call(ctx, NULL, op_new_fn, 1, &size_rval);
    } else {
        gcc_jit_param *new_param = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
        gcc_jit_function *op_new = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "_Znwm", 1, &new_param, 0);
        frame_ptr = gcc_jit_context_new_call(ctx, NULL, op_new, 1, &size_rval);
    }
    
    gcc_jit_lvalue *frame_var = gcc_jit_function_new_local(ramp_func, NULL, void_ptr_type, "coro_frame_ptr");
    gcc_jit_block_add_assignment(entry, NULL, frame_var, frame_ptr);
    
    gcc_jit_function *resume_fn = NULL;
    gcc_jit_function *destroy_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(rout, &resume_fn, &destroy_fn));

    gcc_jit_type *dispatcher_type = gcc_jit_type_get_pointer(gcc_jit_function_as_type(resume_fn));
    
    gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, frame_ptr, gcc_jit_type_get_pointer(frame_type)), NULL);
    gcc_jit_lvalue *f_resume = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 0));
    gcc_jit_lvalue *f_destroy = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 1));
    gcc_jit_lvalue *f_state = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 3));
    
    gcc_jit_block_add_assignment(entry, NULL, f_resume, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(resume_fn, NULL), dispatcher_type));
    gcc_jit_block_add_assignment(entry, NULL, f_destroy, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(destroy_fn, NULL), dispatcher_type));
    
    gcc_jit_rvalue *state_init = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
    gcc_jit_block_add_assignment(entry, NULL, f_state, state_init);
    
    /* Evaluate get_return_object */
    gcc_jit_lvalue *ret_obj = NULL;
    if (descr && descr->get_return_object_call) {
        gcc_jit_type *ret_type = gcc_jit_function_get_return_type(ramp_func);
        ret_obj = gcc_jit_function_new_local(ramp_func, NULL, ret_type, "ret_obj");
        gcc_jit_rvalue *gro_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->get_return_object_call, &gro_rval));
        if (gro_rval) {
            gcc_jit_block_add_assignment(entry, NULL, ret_obj, gro_rval);
        }
    }
    
    /* Evaluate initial_suspend */
    if (descr && descr->initial_suspend_call) {
        gcc_jit_rvalue *is_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->initial_suspend_call, &is_rval));
        if (is_rval) {
            gcc_jit_block_add_eval(entry, NULL, is_rval);
        }
    }
    
    gcc_jit_block_end_with_jump(entry, NULL, alloc_done);
    
    if (gcc_jit_function_get_return_type(ramp_func) == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
        gcc_jit_block_end_with_void_return(alloc_done, NULL);
    } else {
        gcc_jit_rvalue *ret_val = ret_obj ? gcc_jit_lvalue_as_rvalue(ret_obj) : gcc_jit_context_null(ctx, gcc_jit_function_get_return_type(ramp_func));
        gcc_jit_block_end_with_return(alloc_done, NULL, ret_val);
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers coroutine dispatchers.
 * @details Stubs coroutine dispatchers.
 * @param[in] rout Routine.
 * @param[out] out_resume Resume fn.
 * @param[out] out_destroy Destroy fn.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_dispatchers(a_routine_ptr rout, struct gcc_jit_function **out_resume, struct gcc_jit_function **out_destroy) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_resume || !out_destroy) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    char res_name[128];
    snprintf(res_name, sizeof(res_name), "_Z_coro_resume_%p", (void *)rout);
    gcc_jit_param *p_res = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame");
    gcc_jit_function *resume_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, res_name, 1, &p_res, 0);

    char dest_name[128];
    snprintf(dest_name, sizeof(dest_name), "_Z_coro_destroy_%p", (void *)rout);
    gcc_jit_param *p_dest = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame");
    gcc_jit_function *destroy_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, dest_name, 1, &p_dest, 0);

    gcc_jit_block *res_entry = gcc_jit_function_new_block(resume_fn, "entry");
    gcc_jit_type *frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(rout, &frame_type));
    gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, gcc_jit_param_as_rvalue(p_res), gcc_jit_type_get_pointer(frame_type)), NULL);
    gcc_jit_rvalue *state_val = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 3)));
    
    gcc_jit_block *res_default = gcc_jit_function_new_block(resume_fn, "default_state");
    gcc_jit_block_end_with_void_return(res_default, NULL);
    
    gcc_jit_block *res_state0 = gcc_jit_function_new_block(resume_fn, "state_0");
    gcc_jit_block_end_with_void_return(res_state0, NULL);
    
    gcc_jit_case *c0 = gcc_jit_context_new_case(ctx, gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0),
                                                     gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0), res_state0);
    gcc_jit_case *cases[] = { c0 };
    gcc_jit_block_end_with_switch(res_entry, NULL, state_val, res_default, 1, cases);

    gcc_jit_block *dest_entry = gcc_jit_function_new_block(destroy_fn, "entry");
    gcc_jit_param *del_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
    gcc_jit_function *op_del = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPv", 1, &del_param, 0);
    gcc_jit_rvalue *frame_val = gcc_jit_param_as_rvalue(p_dest);
    gcc_jit_block_add_eval(dest_entry, NULL, gcc_jit_context_new_call(ctx, NULL, op_del, 1, &frame_val));
    gcc_jit_block_end_with_void_return(dest_entry, NULL);

    *out_resume = resume_fn;
    *out_destroy = destroy_fn;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
