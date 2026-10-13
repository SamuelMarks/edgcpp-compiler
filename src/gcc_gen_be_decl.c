/**
 * @file gcc_gen_be_decl.c
 * @brief Implementation of declaration lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_dtor.h"
#include "expr.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




/**
 * @brief Retrieves or creates the global dynamic initialization block.
 *
 * This lazily creates the `__edg_global_init` function which acts as
 * the constructor for global dynamic initializers.
 *
 * @param out_block A pointer to receive the gcc_jit_block.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    
    gcc_gen_be_context_t *state = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&state));
    
    if (!state->global_ctor_func) {
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        state->global_ctor_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "__edg_global_init", 0, NULL, 0);
        state->global_ctor_block = gcc_jit_function_new_block(state->global_ctor_func, "entry");
        
        /* Put in .init_array */
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_rvalue *func_addr = gcc_jit_function_get_address(state->global_ctor_func, NULL);
        gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
        gcc_jit_lvalue *init_array_entry = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, void_ptr_type, "__edg_global_init_array_entry");
        gcc_jit_global_set_initializer_rvalue(init_array_entry, func_ptr);
        
        if (p_gcc_jit_lvalue_set_link_section) {
#if defined(__APPLE__) && defined(__MACH__)
            p_gcc_jit_lvalue_set_link_section(init_array_entry, "__DATA,__mod_init_func");
#else
            p_gcc_jit_lvalue_set_link_section(init_array_entry, ".init_array");
#endif
        }
    }
    *out_block = state->global_ctor_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG variable declaration into a libgccjit global lvalue.
 *
 * This handles creating the global variable, applying linkage, TLS attributes,
 * and processing static or dynamic initializers.
 *
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!var) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_lval = (gcc_jit_lvalue *)cached;
      return GCC_GEN_BE_SUCCESS;
  }
  
  gcc_jit_type *var_type = NULL;
  err = gcc_gen_be_lower_type(var->type, &var_type);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  char auto_name[64];
  const char *name = var->source_corresp.name;
  if (!name) {
      snprintf(auto_name, sizeof(auto_name), "__edg_anon_var_%p", (void *)var);
      name = auto_name;
  }
  
  enum gcc_jit_global_kind linkage;
  if (!var->source_corresp.name) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else if (var->storage_class == sc_extern && var->init_kind == initk_none) {
      linkage = GCC_JIT_GLOBAL_IMPORTED;
  } else if (var->storage_class == sc_static) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else {
      linkage = GCC_JIT_GLOBAL_EXPORTED;
  }
  
  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  gcc_jit_lvalue *global = gcc_jit_context_new_global(ctx, loc, linkage, var_type, name);
  
  extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var, global));
  
#if defined(GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC)
  if (var->is_thread_local && p_gcc_jit_lvalue_set_tls_model) {
      enum gcc_jit_tls_model tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;

      extern a_boolean gcc_be_fPIC;
      extern a_boolean gcc_be_fPIE;
      a_boolean pic_enabled = gcc_be_fPIC || gcc_be_fPIE;

      if (linkage == GCC_JIT_GLOBAL_EXPORTED) {
          tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;
      } else if (linkage == GCC_JIT_GLOBAL_INTERNAL) {
          if (!pic_enabled) {
              tls_model = GCC_JIT_TLS_MODEL_LOCAL_EXEC;
          } else {
              tls_model = GCC_JIT_TLS_MODEL_LOCAL_DYNAMIC;
          }
      } else if (linkage == GCC_JIT_GLOBAL_IMPORTED) {
          if (!pic_enabled) {
              tls_model = GCC_JIT_TLS_MODEL_INITIAL_EXEC;
          } else {
              tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;
          }
      }
      p_gcc_jit_lvalue_set_tls_model(global, tls_model);
  }
#endif

  if (var_type && var->type->kind == tk_typeref && (var->type->variant.typeref.qualifiers & TQ_CONST)) {
#if defined(LIBGCCJIT_HAVE_gcc_jit_global_set_readonly)
      gcc_jit_global_set_readonly(global);
#endif
  }
  
  if (var->init_kind == initk_zero) {
      gcc_jit_rvalue *zero_val = NULL;
      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_val));
      if (zero_val) {
          gcc_jit_global_set_initializer_rvalue(global, zero_val);
      }
  } else if (var->init_kind == initk_static) {
      a_constant_ptr init_con = var->initializer.constant;
      if (init_con) {
          a_type_ptr vtype_stripped = var->type;
          while (vtype_stripped && vtype_stripped->kind == tk_typeref) {
              vtype_stripped = vtype_stripped->variant.typeref.type;
          }
          if (vtype_stripped && vtype_stripped->kind == tk_array && init_con->kind == ck_string) {
              size_t total_size = (size_t)vtype_stripped->size;
              if (total_size == 0 || total_size < (size_t)init_con->variant.string.length) {
                  total_size = (size_t)init_con->variant.string.length;
              }
              char *padded = (char *)calloc(1, total_size);
              if (padded) {
                  memcpy(padded, init_con->variant.string.value, (size_t)init_con->variant.string.length);
                  gcc_jit_global_set_initializer(global, padded, total_size);
                  free(padded);
              }
          } else {
              gcc_jit_rvalue *init_rval = NULL;
              extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(init_con, var_type, &init_rval));
              if (init_rval) {
                  gcc_jit_type *init_type = gcc_jit_rvalue_get_type(init_rval);
                  if (init_type != var_type) {
#if defined(LIBGCCJIT_HAVE_gcc_jit_context_new_bitcast)
                      init_rval = gcc_jit_context_new_bitcast(ctx, NULL, init_rval, var_type);
#else
                      init_rval = gcc_jit_context_new_cast(ctx, NULL, init_rval, var_type);
#endif
                  }
                  if (init_rval) {
                      gcc_jit_global_set_initializer_rvalue(global, init_rval);
                  }
              }
          }
      } else {
          gcc_jit_rvalue *zero_val = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_val));
          if (zero_val) {
              gcc_jit_global_set_initializer_rvalue(global, zero_val);
          }
      }
  } else if (var->init_kind == initk_dynamic) {
      if (var->initializer.dynamic && var->initializer.dynamic->kind == dik_expression && var->initializer.dynamic->variant.expression) {
          gcc_jit_rvalue *init_rval = NULL;
          extern gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(var->initializer.dynamic->variant.expression, &init_rval));
          if (init_rval) {
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
              gcc_jit_block_add_assignment(cblock, NULL, global, init_rval);
          }
      }
      if (var->initializer.dynamic && var->initializer.dynamic->destructor) {
          void *cached_dtor = NULL;
          err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, var->initializer.dynamic->destructor, &cached_dtor);
          if (err == GCC_GEN_BE_SUCCESS && cached_dtor) {
              gcc_jit_block *orig_block = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_block));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblock));
              
              /* Call __cxa_atexit / __cxa_thread_atexit via imported functions */
              gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
              gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "f");
              gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "obj");
              gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dso_handle");
              gcc_jit_param *atexit_params[] = {p1, p2, p3};
              
              const char *atexit_name = var->is_thread_local ? "__cxa_thread_atexit" : "__cxa_atexit";
              gcc_jit_function *atexit_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, atexit_name, 3, atexit_params, 0);
              
              gcc_jit_rvalue *func_addr = gcc_jit_function_get_address((gcc_jit_function*)cached_dtor, NULL);
              gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
              gcc_jit_rvalue *obj_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(global, NULL), void_ptr_type);
              gcc_jit_rvalue *dso_handle = gcc_jit_context_null(ctx, void_ptr_type);
              
              gcc_jit_rvalue *atexit_args[] = {func_ptr, obj_ptr, dso_handle};
              gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, atexit_func, 3, atexit_args));
              
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(orig_block));
          }
      }
  }
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, global));
  *out_lval = global;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG variable declaration into a libgccjit local lvalue.
 *
 * @param func The libgccjit function the local belongs to.
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!var) return GCC_GEN_BE_SUCCESS;

  if (var->storage_class == sc_static) {
      /* Static locals are implemented as global variables */
      return gcc_gen_be_lower_global_variable_decl(var, out_lval);
  }
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_lval = (gcc_jit_lvalue *)cached;
      return GCC_GEN_BE_SUCCESS;
  }
  
  gcc_jit_type *var_type = NULL;
  err = gcc_gen_be_lower_type(var->type, &var_type);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  const char *name = var->source_corresp.name;
  if (!name) name = "unnamed_local";

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  gcc_jit_lvalue *local = gcc_jit_function_new_local(func, loc, var_type, name);
  
  extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var, local));
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, local));
  *out_lval = local;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG function/routine declaration into a libgccjit function.
 *
 * Handles creation of the function signature, params, and caches the result.
 *
 * @param rout The frontend routine node.
 * @param out_func A pointer to a gcc_jit_function pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_func` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
  if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_func = NULL;
  if (!rout) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_func = (gcc_jit_function *)cached;
      return GCC_GEN_BE_SUCCESS;
  }
  
  gcc_jit_type *ret_type = NULL;
  a_boolean uses_sret = FALSE;
  gcc_jit_type *sret_ptr_type = NULL;
  if (rout->type && rout->type->kind == tk_routine) {
      a_type_ptr frontend_ret = rout->type->variant.routine.return_type;
      extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
      err = gcc_gen_be_is_returned_via_sret(frontend_ret, &uses_sret);
      if (err != GCC_GEN_BE_SUCCESS) return err;
      if (uses_sret) {
          ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
          gcc_jit_type *actual_ret = NULL;
          err = gcc_gen_be_lower_type(frontend_ret, &actual_ret);
          if (err != GCC_GEN_BE_SUCCESS) return err;
          sret_ptr_type = gcc_jit_type_get_pointer(actual_ret);
      } else {
          err = gcc_gen_be_lower_type(frontend_ret, &ret_type);
          if (err != GCC_GEN_BE_SUCCESS) return err;
      }
  } else {
      ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
  }

  a_const_char *name = rout->source_corresp.name;
  if (!name) name = "unnamed_func";
  
  enum gcc_jit_function_kind linkage;
  if (rout->function_def_number == NULL_function_def_number) {
      linkage = GCC_JIT_FUNCTION_IMPORTED;
  } else if (rout->storage_class == sc_static) {
      linkage = GCC_JIT_FUNCTION_INTERNAL;
  } else {
      linkage = GCC_JIT_FUNCTION_EXPORTED;
  }
  
  int num_params = 0;
  if (uses_sret) num_params++;
  
  a_param_type_ptr ptp;
  if (rout->type && rout->type->kind == tk_routine) {
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          num_params++;
      }
  }
  
  gcc_jit_param **params = NULL;
  if (num_params > 0) {
      params = (gcc_jit_param **)malloc((size_t)num_params * sizeof(gcc_jit_param *));
      if (!params) return GCC_GEN_BE_ERROR_OOM;
      
      int i = 0;
      if (uses_sret) {
          params[i++] = gcc_jit_context_new_param(ctx, NULL, sret_ptr_type, "sret_buffer");
      }
      
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = (rout->function_def_number != NULL_function_def_number) ? scope_for_routine_or_null(rout) : NULL;
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          gcc_jit_type *param_type = NULL;
          a_type_ptr ptype = ptp->type;
          while (ptype && ptype->kind == tk_typeref) {
              ptype = ptype->variant.typeref.type;
          }
          if (ptype && ptype->kind == tk_array) {
              gcc_jit_type *elem_type = NULL;
              err = gcc_gen_be_lower_type(ptype->variant.array.element_type, &elem_type);
              if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
              param_type = gcc_jit_type_get_pointer(elem_type);
          } else {
              err = gcc_gen_be_lower_type(ptype, &param_type);
              if (err != GCC_GEN_BE_SUCCESS) {
                  free(params);
                  return err;
              }
              param_type = gcc_jit_type_unqualified(param_type);
              extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
              a_boolean is_invis_ref = FALSE;
              err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (is_invis_ref) {
                  param_type = gcc_jit_type_get_pointer(param_type);
              }
          }
          
          const char *param_name = "unnamed_param";
          gcc_jit_location *param_loc = NULL;
          if (var_param) {
              if (var_param->source_corresp.name) param_name = var_param->source_corresp.name;
              gcc_gen_be_get_location_from_var(var_param, &param_loc);
              var_param = var_param->next;
          }
          params[i] = gcc_jit_context_new_param(ctx, param_loc, param_type, param_name);
          i++;
      }
  }

  int is_variadic = 0;
  if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info->has_ellipsis) {
      is_variadic = 1;
  }

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&rout->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) {
      if (params) free(params);
      return err;
  }

  gcc_jit_function *func = gcc_jit_context_new_function(
      ctx, loc, linkage, ret_type, name, num_params, params, is_variadic);
  extern gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(rout, func));
      
#if defined(LIBGCCJIT_HAVE_ATTRIBUTES)
  if (rout->is_weak) {
      gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_WEAK);
  }
  if (rout->is_inline) {
      gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_INLINE);
  }
#endif
      
  if (num_params > 0) {
      int i = 0;
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = (rout->function_def_number != NULL_function_def_number) ? scope_for_routine_or_null(rout) : NULL;
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          if (var_param) {
              gcc_jit_lvalue *param_lval = gcc_jit_param_as_lvalue(params[i]);
              
              extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
              GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var_param, param_lval));
              
              extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
              a_type_ptr ptype = ptp->type;
              while (ptype && ptype->kind == tk_typeref) ptype = ptype->variant.typeref.type;
              a_boolean is_invis_ref = FALSE;
              err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (is_invis_ref) {
                  param_lval = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(param_lval), NULL);
              }
              
              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var_param, param_lval));
              var_param = var_param->next;
          }
          i++;
      }
      free(params);
  }
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, func));
  
  if (rout->function_def_number != NULL_function_def_number) {
      a_scope_ptr def_scope = scope_for_routine_or_null(rout);
      if (def_scope && def_scope->variant.routine.statement) {
          gcc_jit_block *entry = gcc_jit_function_new_block(func, "entry");
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));
          
          extern gcc_gen_be_error_t gcc_gen_be_begin_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_end_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT;
          
          GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(func));
          err = gcc_gen_be_lower_routine_body(def_scope->variant.routine.statement, func, rout->variant.routine.exception_specification);
          if (err != GCC_GEN_BE_SUCCESS) return err;
          GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(func));
          
          gcc_jit_block *current = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current));
          if (current) {
              /* If flow reaches end of function without return, and it's void, add void return.
                 Otherwise, libgccjit will error out. */
              if (ret_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                  gcc_jit_block_end_with_void_return(current, NULL);
              } else {
                  /* To be completely safe, maybe return zero or unreachable, but EDG usually guarantees return.
                     However, if the block isn't terminated, we just return zero. */
                  gcc_jit_rvalue *zero = gcc_jit_context_zero(ctx, ret_type);
                  gcc_jit_block_end_with_return(current, NULL, zero);
              }
          }
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
      }
  }
  
  *out_func = func;
  return GCC_GEN_BE_SUCCESS;
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */