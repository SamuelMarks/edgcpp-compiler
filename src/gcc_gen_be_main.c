/**
 * @file gcc_gen_be_main.c
 * @brief Implementation of main entry points for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_main.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_stmt.h"
#include "host_envir.h"
#include "fe_common.h"
#include "error.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_write.h"
#include "il_read.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include <libgccjit.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




gcc_gen_be_error_t gcc_gen_be(void) GCC_GEN_BE_NOEXCEPT {
  a_scope_ptr scope;
  a_routine_ptr rout;
  a_variable_ptr var;

  gcc_gen_be_error_t err = gcc_gen_be_init();
  if (err != GCC_GEN_BE_SUCCESS) {
      const char *err_msg = NULL;
      (void)gcc_gen_be_error_string(err, &err_msg);
      fprintf(f_error, "gcc_gen_be init error: %s\n", err_msg ? err_msg : "Unknown");
      diagnostic_counters.total.catastrophes++;
      return err;
  }
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) {
      fprintf(f_error, "gcc_gen_be init error: Context is NULL\n");
      diagnostic_counters.total.catastrophes++;
      return GCC_GEN_BE_ERROR_INTERNAL;
  }

  scope = il_header.primary_scope;
  
  if (scope) {
    for (var = scope->variables; var != NULL; var = var->next) {
        if (ignore_variable_in_back_end(var)) continue;
        gcc_jit_lvalue *lval = NULL;
        err = gcc_gen_be_lower_global_variable_decl(var, &lval);
        if (err != GCC_GEN_BE_SUCCESS) {
            const char *err_msg = NULL;
            (void)gcc_gen_be_error_string(err, &err_msg);
            fprintf(f_error, "gcc_gen_be variable lowering error: %s\n", err_msg ? err_msg : "Unknown");
            diagnostic_counters.total.errors++;
        }
    }
    
    for (rout = scope->routines; rout != NULL; rout = rout->next) {
      if (ignore_routine_in_back_end(rout)) continue;

#if IL_SHOULD_BE_WRITTEN_TO_FILE
      if (rout->function_def_number != NULL_function_def_number) {
        a_memory_region_number scope_region_number = mem_region_for_routine(rout);
        if (mem_region_table[scope_region_number] == NULL) {
          read_memory_region(scope_region_number);
        }
      }
#endif

      gcc_jit_function *func = NULL;
      err = gcc_gen_be_lower_function_decl(rout, &func);
      if (err != GCC_GEN_BE_SUCCESS || !func) {
          const char *err_msg = NULL;
          (void)gcc_gen_be_error_string(err, &err_msg);
          fprintf(f_error, "gcc_gen_be function lowering error: %s\n", err_msg ? err_msg : "Unknown");
          diagnostic_counters.total.errors++;
          continue;
      }

      if (rout->function_def_number != NULL_function_def_number) {
        a_scope_ptr def_scope = scope_for_routine(rout);
        if (def_scope && def_scope->assoc_block) {
          gcc_jit_block *block = gcc_jit_function_new_block(func, "entry");
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(block)); 

          /* Push the function-level scope */
          GCC_GEN_BE_CHECK(gcc_gen_be_push_scope());

          err = gcc_gen_be_lower_statement(def_scope->assoc_block, func);

          GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope());

          gcc_jit_block *last_block = NULL;
          (void)gcc_gen_be_get_current_block(&last_block);
          if (last_block) {
              a_type_ptr ret_t = rout->type ? rout->type->variant.routine.return_type : NULL;
              while (ret_t && ret_t->kind == tk_typeref) ret_t = ret_t->variant.typeref.type;
              if (!ret_t || ret_t->kind == tk_void) {
                  gcc_jit_block_end_with_void_return(last_block, NULL);
              } else {
                  gcc_jit_rvalue *zero_ret = NULL;
                  (void)gcc_gen_be_get_zero_rvalue(ret_t, NULL, &zero_ret);
                  if (!zero_ret) zero_ret = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_block_end_with_return(last_block, NULL, zero_ret);
              }
              (void)gcc_gen_be_set_current_block(NULL);
          }

          if (err != GCC_GEN_BE_SUCCESS) {
              const char *err_msg = NULL;
              (void)gcc_gen_be_error_string(err, &err_msg);
              fprintf(f_error, "gcc_gen_be statement lowering error: %s\n", err_msg ? err_msg : "Unknown");
              diagnostic_counters.total.errors++;
          }
        }
      }
    }
  }

  
  if (primary_source_file_name && diagnostic_counters.total.errors == 0 && diagnostic_counters.total.catastrophes == 0) {
    a_const_char *obj_name = gcc_be_output_file_name ? gcc_be_output_file_name : derived_name(primary_source_file_name, ".o");
    
    enum gcc_jit_output_kind output_kind = GCC_JIT_OUTPUT_KIND_OBJECT_FILE;
    if (obj_name) {
        size_t len = strlen(obj_name);
        if (len >= 2 && obj_name[len-2] == '.' && obj_name[len-1] == 's') {
            output_kind = GCC_JIT_OUTPUT_KIND_ASSEMBLER;
        } else if (len >= 3 && obj_name[len-3] == '.' && obj_name[len-2] == 's' && obj_name[len-1] == 'o') {
            output_kind = GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY;
        } else if (len >= 4 && obj_name[len-4] == '.' && obj_name[len-3] == 'd' && obj_name[len-2] == 'l' && obj_name[len-1] == 'l') {
            output_kind = GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY;
        } else if (len >= 4 && obj_name[len-4] == '.' && obj_name[len-3] == 'e' && obj_name[len-2] == 'x' && obj_name[len-1] == 'e') {
            output_kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
        } else if (len >= 4 && obj_name[len-4] == '.' && obj_name[len-3] == 'o' && obj_name[len-2] == 'u' && obj_name[len-1] == 't') {
            output_kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
        } else if (!strchr(obj_name, '.')) {
            output_kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
        }
    }
    
    gcc_jit_context_compile_to_file(ctx, output_kind, obj_name);
    
    const char *err_str = gcc_jit_context_get_first_error(ctx);
    if (err_str) {
        fprintf(f_error, "gcc_gen_be compilation error: %s\n", err_str);
        diagnostic_counters.total.catastrophes++;
        return GCC_GEN_BE_ERROR_COMPILATION_FAILED;
    }
  }

  GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

  return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT {
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  primary_source_file_name = NULL;
  if (!skip_il_read) {
    il_read(f_il_output);
  }
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  return gcc_gen_be();
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */