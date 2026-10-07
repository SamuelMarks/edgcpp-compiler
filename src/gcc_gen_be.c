/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#include "fe_common.h"
#include "gcc_gen_be.h"

#if BACK_END_IS_GCC_GEN_BE

#include "il.h"
#include "il_def.h"
#include "types.h"
#include "decls.h"
#include "expr.h"
#include "statements.h"
#include <libgccjit.h>
#include "cmd_line.h"
#include "error.h"

BEGIN_EDG_NAMESPACE

static gcc_jit_context *gcc_jit_ctx = NULL;
static gcc_jit_block *current_block = NULL;

static gcc_jit_location *get_location(a_source_position *pos) {
    if (!pos || pos->seq == 0) return NULL;
    
    a_const_char *file_name = NULL;
    a_const_char *full_name = NULL;
    a_line_number line_number = 0;
    a_boolean at_end = FALSE;
    
    a_source_file_ptr sfp = conv_seq_to_file_and_line(pos->seq, &file_name, &full_name, &line_number, &at_end);
    if (!file_name) return NULL;
    
    return gcc_jit_context_new_location(gcc_jit_ctx, file_name, line_number, pos->column);
}

static gcc_jit_location *get_location_from_expr(an_expr_node_ptr expr) {
    if (!expr) return NULL;
    /* Source positions for expressions are typically stored in the source_corresp or similar fields, but EDG's AST doesn't always have one simple position. 
       Let's stick to NULL for now or try to extract it if available. */
    return NULL;
}

typedef struct be_cache_entry {
    void *key;
    void *value;
    struct be_cache_entry *next;
} be_cache_entry;

#define MAP_SIZE 1024

static be_cache_entry *type_cache[MAP_SIZE];
static be_cache_entry *var_cache[MAP_SIZE];
static be_cache_entry *func_cache[MAP_SIZE];
static be_cache_entry *label_cache[MAP_SIZE];

static void *cache_lookup(be_cache_entry **cache, void *key) {
    size_t hash = (((size_t)key) >> 3) % MAP_SIZE;
    be_cache_entry *e = cache[hash];
    while (e) {
        if (e->key == key) return e->value;
        e = e->next;
    }
    return NULL;
}

static void cache_insert(be_cache_entry **cache, void *key, void *value) {
    size_t hash = (((size_t)key) >> 3) % MAP_SIZE;
    be_cache_entry *e = (be_cache_entry *)malloc(sizeof(be_cache_entry));
    e->key = key;
    e->value = value;
    e->next = cache[hash];
    cache[hash] = e;
}

static void cache_clear(be_cache_entry **cache) {
    for (int i = 0; i < MAP_SIZE; i++) {
        be_cache_entry *e = cache[i];
        while (e) {
            be_cache_entry *next = e->next;
            free(e);
            e = next;
        }
        cache[i] = NULL;
    }
}


#if defined(_WIN32)
#include <windows.h>
static HMODULE libgccjit_handle = NULL;
static void *p_gcc_jit_context_acquire = NULL;
#define gcc_jit_context_acquire (...) ((__typeof__(gcc_jit_context_acquire) *)p_gcc_jit_context_acquire)(__VA_ARGS__)
static void *p_gcc_jit_context_release = NULL;
#define gcc_jit_context_release (...) ((__typeof__(gcc_jit_context_release) *)p_gcc_jit_context_release)(__VA_ARGS__)
static void *p_gcc_jit_context_set_int_option = NULL;
#define gcc_jit_context_set_int_option (...) ((__typeof__(gcc_jit_context_set_int_option) *)p_gcc_jit_context_set_int_option)(__VA_ARGS__)
static void *p_gcc_jit_context_set_bool_option = NULL;
#define gcc_jit_context_set_bool_option (...) ((__typeof__(gcc_jit_context_set_bool_option) *)p_gcc_jit_context_set_bool_option)(__VA_ARGS__)
static void *p_gcc_jit_context_set_str_option = NULL;
#define gcc_jit_context_set_str_option (...) ((__typeof__(gcc_jit_context_set_str_option) *)p_gcc_jit_context_set_str_option)(__VA_ARGS__)
static void *p_gcc_jit_context_add_command_line_option = NULL;
#define gcc_jit_context_add_command_line_option (...) ((__typeof__(gcc_jit_context_add_command_line_option) *)p_gcc_jit_context_add_command_line_option)(__VA_ARGS__)
static void *p_gcc_jit_context_get_type = NULL;
#define gcc_jit_context_get_type (...) ((__typeof__(gcc_jit_context_get_type) *)p_gcc_jit_context_get_type)(__VA_ARGS__)
static void *p_gcc_jit_type_get_pointer = NULL;
#define gcc_jit_type_get_pointer (...) ((__typeof__(gcc_jit_type_get_pointer) *)p_gcc_jit_type_get_pointer)(__VA_ARGS__)
static void *p_gcc_jit_type_get_const = NULL;
#define gcc_jit_type_get_const (...) ((__typeof__(gcc_jit_type_get_const) *)p_gcc_jit_type_get_const)(__VA_ARGS__)
static void *p_gcc_jit_type_get_volatile = NULL;
#define gcc_jit_type_get_volatile (...) ((__typeof__(gcc_jit_type_get_volatile) *)p_gcc_jit_type_get_volatile)(__VA_ARGS__)
static void *p_gcc_jit_context_new_array_type = NULL;
#define gcc_jit_context_new_array_type (...) ((__typeof__(gcc_jit_context_new_array_type) *)p_gcc_jit_context_new_array_type)(__VA_ARGS__)
static void *p_gcc_jit_context_new_field = NULL;
#define gcc_jit_context_new_field (...) ((__typeof__(gcc_jit_context_new_field) *)p_gcc_jit_context_new_field)(__VA_ARGS__)
static void *p_gcc_jit_context_new_bitfield = NULL;
#define gcc_jit_context_new_bitfield (...) ((__typeof__(gcc_jit_context_new_bitfield) *)p_gcc_jit_context_new_bitfield)(__VA_ARGS__)
static void *p_gcc_jit_context_new_struct_type = NULL;
#define gcc_jit_context_new_struct_type (...) ((__typeof__(gcc_jit_context_new_struct_type) *)p_gcc_jit_context_new_struct_type)(__VA_ARGS__)
static void *p_gcc_jit_context_new_opaque_struct = NULL;
#define gcc_jit_context_new_opaque_struct (...) ((__typeof__(gcc_jit_context_new_opaque_struct) *)p_gcc_jit_context_new_opaque_struct)(__VA_ARGS__)
static void *p_gcc_jit_struct_set_fields = NULL;
#define gcc_jit_struct_set_fields (...) ((__typeof__(gcc_jit_struct_set_fields) *)p_gcc_jit_struct_set_fields)(__VA_ARGS__)
static void *p_gcc_jit_context_new_union_type = NULL;
#define gcc_jit_context_new_union_type (...) ((__typeof__(gcc_jit_context_new_union_type) *)p_gcc_jit_context_new_union_type)(__VA_ARGS__)
static void *p_gcc_jit_context_new_function_ptr_type = NULL;
#define gcc_jit_context_new_function_ptr_type (...) ((__typeof__(gcc_jit_context_new_function_ptr_type) *)p_gcc_jit_context_new_function_ptr_type)(__VA_ARGS__)
static void *p_gcc_jit_context_new_param = NULL;
#define gcc_jit_context_new_param (...) ((__typeof__(gcc_jit_context_new_param) *)p_gcc_jit_context_new_param)(__VA_ARGS__)
static void *p_gcc_jit_context_new_function = NULL;
#define gcc_jit_context_new_function (...) ((__typeof__(gcc_jit_context_new_function) *)p_gcc_jit_context_new_function)(__VA_ARGS__)
static void *p_gcc_jit_context_new_global = NULL;
#define gcc_jit_context_new_global (...) ((__typeof__(gcc_jit_context_new_global) *)p_gcc_jit_context_new_global)(__VA_ARGS__)
static void *p_gcc_jit_global_set_initializer = NULL;
#define gcc_jit_global_set_initializer (...) ((__typeof__(gcc_jit_global_set_initializer) *)p_gcc_jit_global_set_initializer)(__VA_ARGS__)
static void *p_gcc_jit_function_new_block = NULL;
#define gcc_jit_function_new_block (...) ((__typeof__(gcc_jit_function_new_block) *)p_gcc_jit_function_new_block)(__VA_ARGS__)
static void *p_gcc_jit_function_new_local = NULL;
#define gcc_jit_function_new_local (...) ((__typeof__(gcc_jit_function_new_local) *)p_gcc_jit_function_new_local)(__VA_ARGS__)
static void *p_gcc_jit_block_add_eval = NULL;
#define gcc_jit_block_add_eval (...) ((__typeof__(gcc_jit_block_add_eval) *)p_gcc_jit_block_add_eval)(__VA_ARGS__)
static void *p_gcc_jit_block_add_assignment = NULL;
#define gcc_jit_block_add_assignment (...) ((__typeof__(gcc_jit_block_add_assignment) *)p_gcc_jit_block_add_assignment)(__VA_ARGS__)
static void *p_gcc_jit_block_add_assignment_op = NULL;
#define gcc_jit_block_add_assignment_op (...) ((__typeof__(gcc_jit_block_add_assignment_op) *)p_gcc_jit_block_add_assignment_op)(__VA_ARGS__)
static void *p_gcc_jit_block_end_with_conditional = NULL;
#define gcc_jit_block_end_with_conditional (...) ((__typeof__(gcc_jit_block_end_with_conditional) *)p_gcc_jit_block_end_with_conditional)(__VA_ARGS__)
static void *p_gcc_jit_block_end_with_jump = NULL;
#define gcc_jit_block_end_with_jump (...) ((__typeof__(gcc_jit_block_end_with_jump) *)p_gcc_jit_block_end_with_jump)(__VA_ARGS__)
static void *p_gcc_jit_block_end_with_return = NULL;
#define gcc_jit_block_end_with_return (...) ((__typeof__(gcc_jit_block_end_with_return) *)p_gcc_jit_block_end_with_return)(__VA_ARGS__)
static void *p_gcc_jit_block_end_with_void_return = NULL;
#define gcc_jit_block_end_with_void_return (...) ((__typeof__(gcc_jit_block_end_with_void_return) *)p_gcc_jit_block_end_with_void_return)(__VA_ARGS__)
static void *p_gcc_jit_block_end_with_switch = NULL;
#define gcc_jit_block_end_with_switch (...) ((__typeof__(gcc_jit_block_end_with_switch) *)p_gcc_jit_block_end_with_switch)(__VA_ARGS__)
static void *p_gcc_jit_context_new_rvalue_from_int = NULL;
#define gcc_jit_context_new_rvalue_from_int (...) ((__typeof__(gcc_jit_context_new_rvalue_from_int) *)p_gcc_jit_context_new_rvalue_from_int)(__VA_ARGS__)
static void *p_gcc_jit_context_new_rvalue_from_long = NULL;
#define gcc_jit_context_new_rvalue_from_long (...) ((__typeof__(gcc_jit_context_new_rvalue_from_long) *)p_gcc_jit_context_new_rvalue_from_long)(__VA_ARGS__)
static void *p_gcc_jit_context_new_rvalue_from_double = NULL;
#define gcc_jit_context_new_rvalue_from_double (...) ((__typeof__(gcc_jit_context_new_rvalue_from_double) *)p_gcc_jit_context_new_rvalue_from_double)(__VA_ARGS__)
static void *p_gcc_jit_context_new_rvalue_from_ptr = NULL;
#define gcc_jit_context_new_rvalue_from_ptr (...) ((__typeof__(gcc_jit_context_new_rvalue_from_ptr) *)p_gcc_jit_context_new_rvalue_from_ptr)(__VA_ARGS__)
static void *p_gcc_jit_context_new_string_literal = NULL;
#define gcc_jit_context_new_string_literal (...) ((__typeof__(gcc_jit_context_new_string_literal) *)p_gcc_jit_context_new_string_literal)(__VA_ARGS__)
static void *p_gcc_jit_context_null = NULL;
#define gcc_jit_context_null (...) ((__typeof__(gcc_jit_context_null) *)p_gcc_jit_context_null)(__VA_ARGS__)
static void *p_gcc_jit_context_zero = NULL;
#define gcc_jit_context_zero (...) ((__typeof__(gcc_jit_context_zero) *)p_gcc_jit_context_zero)(__VA_ARGS__)
static void *p_gcc_jit_context_one = NULL;
#define gcc_jit_context_one (...) ((__typeof__(gcc_jit_context_one) *)p_gcc_jit_context_one)(__VA_ARGS__)
static void *p_gcc_jit_context_new_unary_op = NULL;
#define gcc_jit_context_new_unary_op (...) ((__typeof__(gcc_jit_context_new_unary_op) *)p_gcc_jit_context_new_unary_op)(__VA_ARGS__)
static void *p_gcc_jit_context_new_binary_op = NULL;
#define gcc_jit_context_new_binary_op (...) ((__typeof__(gcc_jit_context_new_binary_op) *)p_gcc_jit_context_new_binary_op)(__VA_ARGS__)
static void *p_gcc_jit_context_new_comparison = NULL;
#define gcc_jit_context_new_comparison (...) ((__typeof__(gcc_jit_context_new_comparison) *)p_gcc_jit_context_new_comparison)(__VA_ARGS__)
static void *p_gcc_jit_context_new_cast = NULL;
#define gcc_jit_context_new_cast (...) ((__typeof__(gcc_jit_context_new_cast) *)p_gcc_jit_context_new_cast)(__VA_ARGS__)
static void *p_gcc_jit_context_new_array_access = NULL;
#define gcc_jit_context_new_array_access (...) ((__typeof__(gcc_jit_context_new_array_access) *)p_gcc_jit_context_new_array_access)(__VA_ARGS__)
static void *p_gcc_jit_lvalue_access_field = NULL;
#define gcc_jit_lvalue_access_field (...) ((__typeof__(gcc_jit_lvalue_access_field) *)p_gcc_jit_lvalue_access_field)(__VA_ARGS__)
static void *p_gcc_jit_rvalue_access_field = NULL;
#define gcc_jit_rvalue_access_field (...) ((__typeof__(gcc_jit_rvalue_access_field) *)p_gcc_jit_rvalue_access_field)(__VA_ARGS__)
static void *p_gcc_jit_rvalue_dereference_field = NULL;
#define gcc_jit_rvalue_dereference_field (...) ((__typeof__(gcc_jit_rvalue_dereference_field) *)p_gcc_jit_rvalue_dereference_field)(__VA_ARGS__)
static void *p_gcc_jit_rvalue_dereference = NULL;
#define gcc_jit_rvalue_dereference (...) ((__typeof__(gcc_jit_rvalue_dereference) *)p_gcc_jit_rvalue_dereference)(__VA_ARGS__)
static void *p_gcc_jit_lvalue_get_address = NULL;
#define gcc_jit_lvalue_get_address (...) ((__typeof__(gcc_jit_lvalue_get_address) *)p_gcc_jit_lvalue_get_address)(__VA_ARGS__)
static void *p_gcc_jit_lvalue_as_rvalue = NULL;
#define gcc_jit_lvalue_as_rvalue (...) ((__typeof__(gcc_jit_lvalue_as_rvalue) *)p_gcc_jit_lvalue_as_rvalue)(__VA_ARGS__)
static void *p_gcc_jit_param_as_lvalue = NULL;
#define gcc_jit_param_as_lvalue (...) ((__typeof__(gcc_jit_param_as_lvalue) *)p_gcc_jit_param_as_lvalue)(__VA_ARGS__)
static void *p_gcc_jit_param_as_rvalue = NULL;
#define gcc_jit_param_as_rvalue (...) ((__typeof__(gcc_jit_param_as_rvalue) *)p_gcc_jit_param_as_rvalue)(__VA_ARGS__)
static void *p_gcc_jit_context_new_call = NULL;
#define gcc_jit_context_new_call (...) ((__typeof__(gcc_jit_context_new_call) *)p_gcc_jit_context_new_call)(__VA_ARGS__)
static void *p_gcc_jit_context_new_call_through_ptr = NULL;
#define gcc_jit_context_new_call_through_ptr (...) ((__typeof__(gcc_jit_context_new_call_through_ptr) *)p_gcc_jit_context_new_call_through_ptr)(__VA_ARGS__)
static void *p_gcc_jit_block_add_extended_asm = NULL;
#define gcc_jit_block_add_extended_asm (...) ((__typeof__(gcc_jit_block_add_extended_asm) *)p_gcc_jit_block_add_extended_asm)(__VA_ARGS__)
static void *p_gcc_jit_context_new_location = NULL;
#define gcc_jit_context_new_location (...) ((__typeof__(gcc_jit_context_new_location) *)p_gcc_jit_context_new_location)(__VA_ARGS__)
static void *p_gcc_jit_context_compile_to_file = NULL;
#define gcc_jit_context_compile_to_file (...) ((__typeof__(gcc_jit_context_compile_to_file) *)p_gcc_jit_context_compile_to_file)(__VA_ARGS__)
static void *p_gcc_jit_context_get_first_error = NULL;
#define gcc_jit_context_get_first_error (...) ((__typeof__(gcc_jit_context_get_first_error) *)p_gcc_jit_context_get_first_error)(__VA_ARGS__)

static void load_libgccjit_windows(void) {
  if (libgccjit_handle) return;
  const char* candidates[] = {
      "libgccjit.dll",
      "C:\\msys64\\mingw64\\bin\\libgccjit-0.dll",
      "libgccjit-0.dll"
  };
  for (int i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
      libgccjit_handle = LoadLibraryA(candidates[i]);
      if (libgccjit_handle) break;
  }
  if (!libgccjit_handle) {
      fprintf(f_error, "gcc_gen_be: failed to load libgccjit.dll");
      return;
  }
  p_gcc_jit_context_acquire = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_acquire");
  if (!p_gcc_jit_context_acquire) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_acquire");
  p_gcc_jit_context_release = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_release");
  if (!p_gcc_jit_context_release) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_release");
  p_gcc_jit_context_set_int_option = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_set_int_option");
  if (!p_gcc_jit_context_set_int_option) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_set_int_option");
  p_gcc_jit_context_set_bool_option = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_set_bool_option");
  if (!p_gcc_jit_context_set_bool_option) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_set_bool_option");
  p_gcc_jit_context_set_str_option = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_set_str_option");
  if (!p_gcc_jit_context_set_str_option) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_set_str_option");
  p_gcc_jit_context_add_command_line_option = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_add_command_line_option");
  if (!p_gcc_jit_context_add_command_line_option) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_add_command_line_option");
  p_gcc_jit_context_get_type = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_get_type");
  if (!p_gcc_jit_context_get_type) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_get_type");
  p_gcc_jit_type_get_pointer = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_type_get_pointer");
  if (!p_gcc_jit_type_get_pointer) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_type_get_pointer");
  p_gcc_jit_type_get_const = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_type_get_const");
  if (!p_gcc_jit_type_get_const) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_type_get_const");
  p_gcc_jit_type_get_volatile = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_type_get_volatile");
  if (!p_gcc_jit_type_get_volatile) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_type_get_volatile");
  p_gcc_jit_context_new_array_type = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_array_type");
  if (!p_gcc_jit_context_new_array_type) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_array_type");
  p_gcc_jit_context_new_field = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_field");
  if (!p_gcc_jit_context_new_field) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_field");
  p_gcc_jit_context_new_bitfield = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_bitfield");
  if (!p_gcc_jit_context_new_bitfield) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_bitfield");
  p_gcc_jit_context_new_struct_type = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_struct_type");
  if (!p_gcc_jit_context_new_struct_type) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_struct_type");
  p_gcc_jit_context_new_opaque_struct = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_opaque_struct");
  if (!p_gcc_jit_context_new_opaque_struct) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_opaque_struct");
  p_gcc_jit_struct_set_fields = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_struct_set_fields");
  if (!p_gcc_jit_struct_set_fields) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_struct_set_fields");
  p_gcc_jit_context_new_union_type = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_union_type");
  if (!p_gcc_jit_context_new_union_type) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_union_type");
  p_gcc_jit_context_new_function_ptr_type = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_function_ptr_type");
  if (!p_gcc_jit_context_new_function_ptr_type) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_function_ptr_type");
  p_gcc_jit_context_new_param = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_param");
  if (!p_gcc_jit_context_new_param) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_param");
  p_gcc_jit_context_new_function = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_function");
  if (!p_gcc_jit_context_new_function) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_function");
  p_gcc_jit_context_new_global = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_global");
  if (!p_gcc_jit_context_new_global) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_global");
  p_gcc_jit_global_set_initializer = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_global_set_initializer");
  if (!p_gcc_jit_global_set_initializer) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_global_set_initializer");
  p_gcc_jit_function_new_block = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_function_new_block");
  if (!p_gcc_jit_function_new_block) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_function_new_block");
  p_gcc_jit_function_new_local = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_function_new_local");
  if (!p_gcc_jit_function_new_local) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_function_new_local");
  p_gcc_jit_block_add_eval = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_add_eval");
  if (!p_gcc_jit_block_add_eval) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_add_eval");
  p_gcc_jit_block_add_assignment = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_add_assignment");
  if (!p_gcc_jit_block_add_assignment) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_add_assignment");
  p_gcc_jit_block_add_assignment_op = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_add_assignment_op");
  if (!p_gcc_jit_block_add_assignment_op) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_add_assignment_op");
  p_gcc_jit_block_end_with_conditional = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_end_with_conditional");
  if (!p_gcc_jit_block_end_with_conditional) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_end_with_conditional");
  p_gcc_jit_block_end_with_jump = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_end_with_jump");
  if (!p_gcc_jit_block_end_with_jump) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_end_with_jump");
  p_gcc_jit_block_end_with_return = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_end_with_return");
  if (!p_gcc_jit_block_end_with_return) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_end_with_return");
  p_gcc_jit_block_end_with_void_return = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_end_with_void_return");
  if (!p_gcc_jit_block_end_with_void_return) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_end_with_void_return");
  p_gcc_jit_block_end_with_switch = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_end_with_switch");
  if (!p_gcc_jit_block_end_with_switch) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_end_with_switch");
  p_gcc_jit_context_new_rvalue_from_int = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_rvalue_from_int");
  if (!p_gcc_jit_context_new_rvalue_from_int) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_rvalue_from_int");
  p_gcc_jit_context_new_rvalue_from_long = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_rvalue_from_long");
  if (!p_gcc_jit_context_new_rvalue_from_long) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_rvalue_from_long");
  p_gcc_jit_context_new_rvalue_from_double = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_rvalue_from_double");
  if (!p_gcc_jit_context_new_rvalue_from_double) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_rvalue_from_double");
  p_gcc_jit_context_new_rvalue_from_ptr = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_rvalue_from_ptr");
  if (!p_gcc_jit_context_new_rvalue_from_ptr) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_rvalue_from_ptr");
  p_gcc_jit_context_new_string_literal = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_string_literal");
  if (!p_gcc_jit_context_new_string_literal) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_string_literal");
  p_gcc_jit_context_null = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_null");
  if (!p_gcc_jit_context_null) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_null");
  p_gcc_jit_context_zero = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_zero");
  if (!p_gcc_jit_context_zero) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_zero");
  p_gcc_jit_context_one = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_one");
  if (!p_gcc_jit_context_one) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_one");
  p_gcc_jit_context_new_unary_op = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_unary_op");
  if (!p_gcc_jit_context_new_unary_op) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_unary_op");
  p_gcc_jit_context_new_binary_op = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_binary_op");
  if (!p_gcc_jit_context_new_binary_op) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_binary_op");
  p_gcc_jit_context_new_comparison = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_comparison");
  if (!p_gcc_jit_context_new_comparison) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_comparison");
  p_gcc_jit_context_new_cast = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_cast");
  if (!p_gcc_jit_context_new_cast) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_cast");
  p_gcc_jit_context_new_array_access = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_array_access");
  if (!p_gcc_jit_context_new_array_access) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_array_access");
  p_gcc_jit_lvalue_access_field = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_lvalue_access_field");
  if (!p_gcc_jit_lvalue_access_field) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_lvalue_access_field");
  p_gcc_jit_rvalue_access_field = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_rvalue_access_field");
  if (!p_gcc_jit_rvalue_access_field) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_rvalue_access_field");
  p_gcc_jit_rvalue_dereference_field = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_rvalue_dereference_field");
  if (!p_gcc_jit_rvalue_dereference_field) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_rvalue_dereference_field");
  p_gcc_jit_rvalue_dereference = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_rvalue_dereference");
  if (!p_gcc_jit_rvalue_dereference) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_rvalue_dereference");
  p_gcc_jit_lvalue_get_address = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_lvalue_get_address");
  if (!p_gcc_jit_lvalue_get_address) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_lvalue_get_address");
  p_gcc_jit_lvalue_as_rvalue = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_lvalue_as_rvalue");
  if (!p_gcc_jit_lvalue_as_rvalue) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_lvalue_as_rvalue");
  p_gcc_jit_param_as_lvalue = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_param_as_lvalue");
  if (!p_gcc_jit_param_as_lvalue) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_param_as_lvalue");
  p_gcc_jit_param_as_rvalue = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_param_as_rvalue");
  if (!p_gcc_jit_param_as_rvalue) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_param_as_rvalue");
  p_gcc_jit_context_new_call = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_call");
  if (!p_gcc_jit_context_new_call) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_call");
  p_gcc_jit_context_new_call_through_ptr = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_call_through_ptr");
  if (!p_gcc_jit_context_new_call_through_ptr) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_call_through_ptr");
  p_gcc_jit_block_add_extended_asm = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_block_add_extended_asm");
  if (!p_gcc_jit_block_add_extended_asm) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_block_add_extended_asm");
  p_gcc_jit_context_new_location = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_new_location");
  if (!p_gcc_jit_context_new_location) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_new_location");
  p_gcc_jit_context_compile_to_file = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_compile_to_file");
  if (!p_gcc_jit_context_compile_to_file) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_compile_to_file");
  p_gcc_jit_context_get_first_error = (void *)GetProcAddress(libgccjit_handle, "gcc_jit_context_get_first_error");
  if (!p_gcc_jit_context_get_first_error) fprintf(f_error, "gcc_gen_be: missing symbol %s", "gcc_jit_context_get_first_error");

}
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <dlfcn.h>
static void *libgccjit_handle = NULL;
static void load_libgccjit_posix(void) {
    if (libgccjit_handle) return;
#if defined(__APPLE__)
    const char *lib_name = "libgccjit.dylib";
#else
    const char *lib_name = "libgccjit.so";
#endif
    libgccjit_handle = dlopen(lib_name, RTLD_NOW | RTLD_GLOBAL);
    if (!libgccjit_handle) {
        /* Optionally fallback but typically linked at compile-time */
    }
}
#endif

void gcc_gen_be_early_init(void) {
#if defined(_WIN32)
  load_libgccjit_windows();
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
  load_libgccjit_posix();
#endif
}

void gcc_gen_be_init(void) {
  gcc_jit_ctx = gcc_jit_context_acquire();
  if (!gcc_jit_ctx) {
      fprintf(f_error, "gcc_gen_be: failed to acquire gcc_jit_context");
      return;
  }

  int opt_level = 3;
  gcc_jit_context_set_int_option(gcc_jit_ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, opt_level);
  
  gcc_jit_context_set_bool_option(gcc_jit_ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, 1);

  if (FALSE) {
      gcc_jit_context_set_bool_option(gcc_jit_ctx, GCC_JIT_BOOL_OPTION_DUMP_SUMMARY, 1);
  }
}

void gcc_gen_be_cleanup(void) {
  if (gcc_jit_ctx) {
      gcc_jit_context_release(gcc_jit_ctx);
      gcc_jit_ctx = NULL;
  }
  cache_clear(type_cache);
  cache_clear(var_cache);
  cache_clear(func_cache);
  cache_clear(label_cache);
}


static gcc_jit_type *lower_type(a_type_ptr tp) {
  if (!tp) return NULL;
  
  gcc_jit_type *cached = (gcc_jit_type *)cache_lookup(type_cache, tp);
  if (cached) return cached;
  
  if (tp->kind == tk_typeref) {
    gcc_jit_type *base = lower_type(tp->variant.typeref.type);
    if (tp->variant.typeref.qualifiers & TQ_CONST) {
      base = gcc_jit_type_get_const(base);
    }
    if (tp->variant.typeref.qualifiers & TQ_VOLATILE) {
      base = gcc_jit_type_get_volatile(base);
    }
    cache_insert(type_cache, tp, base);
    return base;
  }

  gcc_jit_type *res = NULL;
  switch (tp->kind) {
    case tk_void:
      res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_VOID); break;
    
    case tk_integer:
      switch (tp->variant.integer.int_kind) {
        case ik_char: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_CHAR); break;
        case ik_signed_char: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_SIGNED_CHAR); break;
        case ik_unsigned_char: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_UNSIGNED_CHAR); break;
        case ik_short: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_SHORT); break;
        case ik_unsigned_short: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_UNSIGNED_SHORT); break;
        case ik_int: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT); break;
        case ik_unsigned_int: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_UNSIGNED_INT); break;
        case ik_long: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_LONG); break;
        case ik_unsigned_long: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_UNSIGNED_LONG); break;
#if LONG_LONG_ALLOWED
        case ik_long_long: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_LONG_LONG); break;
        case ik_unsigned_long_long: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG); break;
#endif
        default: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT); break;
      }
      break;

    case tk_float:
      switch (tp->variant.float_kind) {
        case fk_float: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_FLOAT); break;
        case fk_double: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_DOUBLE); break;
        case fk_long_double: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
        default: res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_DOUBLE); break;
      }
      break;

    case tk_pointer:
      res = gcc_jit_type_get_pointer(lower_type(tp->variant.pointer.type));
      break;
      
    case tk_array:
      if (!tp->variant.array.is_variable_size_array && !tp->variant.array.is_template_dependent_size_array) {
         int size = (int)tp->variant.array.variant.number_of_elements;
         res = gcc_jit_context_new_array_type(gcc_jit_ctx, NULL, lower_type(tp->variant.array.element_type), size);
      } else {
         res = gcc_jit_type_get_pointer(lower_type(tp->variant.array.element_type));
      }
      break;

    case tk_class:
    case tk_struct:
    case tk_union:
      {
         a_const_char *name = tp->source_corresp.name;
         if (!name) name = "unnamed_struct";
         
         gcc_jit_struct *s = gcc_jit_context_new_opaque_struct(gcc_jit_ctx, NULL, name);
         res = gcc_jit_struct_as_type(s);
         cache_insert(type_cache, tp, res);
         
         int num_fields = 0;
         a_field_ptr f;
         for (f = tp->variant.class_struct_union.field_list; f != NULL; f = f->next) {
            num_fields++;
         }
         
         if (num_fields > 0) {
            gcc_jit_field **fields = (gcc_jit_field **)malloc((size_t)num_fields * sizeof(gcc_jit_field*));
            int i = 0;
            for (f = tp->variant.class_struct_union.field_list; f != NULL; f = f->next) {
                a_const_char *fname = f->source_corresp.name;
                if (!fname) fname = "unnamed_field";
                if (f->bit_size > 0) {
                    fields[i] = gcc_jit_context_new_bitfield(gcc_jit_ctx, NULL, lower_type(f->type), f->bit_size, fname);
                } else {
                    fields[i] = gcc_jit_context_new_field(gcc_jit_ctx, NULL, lower_type(f->type), fname);
                }
                i++;
            }
            if (tp->kind == tk_union) {
                res = gcc_jit_context_new_union_type(gcc_jit_ctx, NULL, name, num_fields, fields);
                /* Overwrite cached opaque struct with the actual union */
                be_cache_entry *e = type_cache[(((size_t)tp) >> 3) % MAP_SIZE];
                while (e) {
                    if (e->key == tp) { e->value = res; break; }
                    e = e->next;
                }
            } else {
                gcc_jit_struct_set_fields(s, NULL, num_fields, fields);
            }
            free(fields);
         }
         return res;
      }

    case tk_routine:
      res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_VOID_PTR);
      break;
      
    case tk_ptr_to_member:
      {
         gcc_jit_field *f1 = gcc_jit_context_new_field(gcc_jit_ctx, NULL, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_VOID_PTR), "func_ptr");
         gcc_jit_field *f2 = gcc_jit_context_new_field(gcc_jit_ctx, NULL, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_LONG), "this_adj");
         gcc_jit_field *fields[] = {f1, f2};
         gcc_jit_struct *s = gcc_jit_context_new_struct_type(gcc_jit_ctx, NULL, "ptr_to_member", 2, fields);
         res = gcc_jit_struct_as_type(s);
      }
      break;
      
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      {
         int size = 1;
         if (tp->variant.vector.number_of_elements) {
            size = (int)tp->variant.vector.number_of_elements->variant.integer_value;
         }
         res = gcc_jit_context_new_array_type(gcc_jit_ctx, NULL, lower_type(tp->variant.vector.element_type), size);
      }
      break;
#endif

    case tk_error:
      fprintf(f_error, "gcc_gen_be: encountered tk_error type\n");
      res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_VOID);
      break;
      
    default:
      fprintf(f_error, "gcc_gen_be: unsupported type kind %d\n", tp->kind);
      res = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_VOID);
      break;
  }
  
  if (res) {
      cache_insert(type_cache, tp, res);
  }
  return res;
}

static gcc_jit_lvalue *lower_variable_decl(a_variable_ptr var) {
  if (!var) return NULL;
  
  gcc_jit_lvalue *cached = (gcc_jit_lvalue *)cache_lookup(var_cache, var);
  if (cached) return cached;
  
  gcc_jit_type *var_type = lower_type(var->type);
  const char *name = var->source_corresp.name;
  if (!name) name = "unnamed_var";
  
  enum gcc_jit_global_kind linkage;
  if (var->storage_class == sc_extern && var->init_kind == initk_none) {
      linkage = GCC_JIT_GLOBAL_IMPORTED;
  } else if (var->storage_class == sc_static) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else {
      linkage = GCC_JIT_GLOBAL_EXPORTED;
  }
  
  gcc_jit_lvalue *global = gcc_jit_context_new_global(gcc_jit_ctx, get_location(&var->source_corresp.decl_position), linkage, var_type, name);
  
  if (var->init_kind == initk_zero) {
      gcc_jit_global_set_initializer_rvalue(global, gcc_jit_context_zero(gcc_jit_ctx, var_type));
  } else if (var->init_kind == initk_static) {
      a_constant_ptr init_con = var->initializer.constant;
      /* Stub: full initialization to be handled later */
      if (init_con && init_con->kind == ck_integer) {
         a_boolean err = FALSE;
         long val = (long)value_of_integer_constant(init_con, &err);
         gcc_jit_rvalue *rval = gcc_jit_context_new_rvalue_from_long(gcc_jit_ctx, var_type, val);
         gcc_jit_global_set_initializer_rvalue(global, rval);
      } else {
         gcc_jit_global_set_initializer_rvalue(global, gcc_jit_context_zero(gcc_jit_ctx, var_type));
      }
  }
  
  cache_insert(var_cache, var, global);
  return global;
}

static gcc_jit_block *get_label_block(gcc_jit_function *func, a_label_ptr label) {
    if (!label) return NULL;
    gcc_jit_block *block = (gcc_jit_block *)cache_lookup(label_cache, label);
    if (!block) {
        const char *name = "label";
        if (label->source_corresp.name) {
            name = label->source_corresp.name;
        } else if (label->break_label) {
            name = "break";
        } else if (label->continue_label) {
            name = "continue";
        }
        block = gcc_jit_function_new_block(func, name);
        cache_insert(label_cache, label, block);
    }
    return block;
}

static gcc_jit_block *get_switch_case_block(gcc_jit_function *func, a_switch_case_entry_ptr scep) {
    if (!scep) return NULL;
    gcc_jit_block *block = (gcc_jit_block *)cache_lookup(label_cache, scep);
    if (!block) {
        block = gcc_jit_function_new_block(func, scep->case_value ? "case" : "default");
        cache_insert(label_cache, scep, block);
    }
    return block;
}

static void lower_statement(a_statement_ptr stmt, gcc_jit_function *func);
static gcc_jit_function *lower_function_decl(a_routine_ptr rout);
static gcc_jit_rvalue *lower_expr_rvalue(an_expr_node_ptr expr);

static gcc_jit_lvalue *lower_expr_lvalue(an_expr_node_ptr expr) {
  if (!expr) return NULL;
  
  switch (expr->kind) {
      case enk_variable:
          {
              a_variable_ptr var = expr->variant.variable.ptr;
              gcc_jit_lvalue *lval = (gcc_jit_lvalue *)cache_lookup(var_cache, var);
              if (!lval) {
                  lval = lower_variable_decl(var);
              }
              return lval;
          }
          
      case enk_operation:
          {
              an_expr_operator_kind op = expr->variant.operation.kind;
              an_expr_node_ptr op1 = expr->variant.operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;
              
              if (op == eok_indirect) {
                  gcc_jit_rvalue *ptr = lower_expr_rvalue(op1);
                  /* Note: source location is NULL for now */
                  return gcc_jit_rvalue_dereference(ptr, NULL);
              } else if (op == eok_dot_field) {
                  gcc_jit_lvalue *obj = lower_expr_lvalue(op1);
                  const char *fname = "unnamed_field";
                  if (op2 && op2->kind == enk_field && op2->variant.field.ptr->source_corresp.name) {
                      fname = op2->variant.field.ptr->source_corresp.name;
                  }
                  /* We don't have gcc_jit_field* directly, we need to get it from the struct type.
                     Actually, gcc_jit_lvalue_access_field takes a gcc_jit_field*.
                     We'd need to cache field pointers or find them by name.
                     For now, stub the exact field lookup, we can implement it if required. */
                  /* STUB for field access */
                  return NULL;
              } else if (op == eok_subscript) {
                  gcc_jit_rvalue *ptr = lower_expr_rvalue(op1);
                  gcc_jit_rvalue *idx = lower_expr_rvalue(op2);
                  return gcc_jit_context_new_array_access(gcc_jit_ctx, NULL, ptr, idx);
              }
              /* More lvalue operations */
              return NULL;
          }
          
      default:
          return NULL;
  }
}

static gcc_jit_rvalue *lower_expr_rvalue(an_expr_node_ptr expr) {
  if (!expr) return NULL;
  
  switch (expr->kind) {
          case enk_constant:
          {
              a_constant_ptr con = expr->variant.constant.ptr;
              gcc_jit_type *type = lower_type(expr->type);
              a_boolean err = FALSE;
              if (con->kind == ck_integer) {
                  long val = (long)value_of_integer_constant(con, &err);
                  return gcc_jit_context_new_rvalue_from_long(gcc_jit_ctx, type, val);
              } else if (con->kind == ck_float) {
                  double val = (double)fetch_host_fp_value(expr->type->variant.float_kind, &con->variant.float_value);
                  return gcc_jit_context_new_rvalue_from_double(gcc_jit_ctx, type, val);
              } else if (con->kind == ck_string) {
                  return gcc_jit_context_new_string_literal(gcc_jit_ctx, (const char *)con->variant.string.value);
              }
              return gcc_jit_context_zero(gcc_jit_ctx, type);
          }
          
      case enk_variable:
          {
              gcc_jit_lvalue *lval = lower_expr_lvalue(expr);
              if (lval) return gcc_jit_lvalue_as_rvalue(lval);
              return NULL;
          }
          
      case enk_operation:
          {
              an_expr_operator_kind op = expr->variant.operation.kind;
              an_expr_node_ptr op1 = expr->variant.operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;
              gcc_jit_type *type = lower_type(expr->type);
              
              switch (op) {
                  case eok_assign:
                      {
                          gcc_jit_lvalue *lhs = lower_expr_lvalue(op1);
                          gcc_jit_rvalue *rhs = lower_expr_rvalue(op2);
                          /* Wait, we don't have the block here to add assignment! 
                             EDG lower_statement usually lowers assignment into stmk_expr.
                             But here we are evaluating rvalue. We might need current_block! 
                             For now, this is a conceptual issue with gcc_jit.
                             Assignments *must* be added to a block in gcc_jit. */
                          /* STUB */
                          return rhs;
                      }
                  case eok_add:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_PLUS, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_subtract:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_MINUS, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_multiply:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_MULT, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_divide:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_remainder:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_MODULO, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_shiftl:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_shiftr:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_land:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_AND, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_lor:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_OR, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_comma:
                      {
                          gcc_jit_rvalue *a = lower_expr_rvalue(op1);
                          if (a && current_block) gcc_jit_block_add_eval(current_block, NULL, a);
                          return lower_expr_rvalue(op2);
                      }
                  case eok_question:
                      {
                          gcc_jit_rvalue *cond = lower_expr_rvalue(op1);
                          an_expr_node_ptr op3 = op2 ? op2->next : NULL;
                          if (!current_block) return NULL;
                          
                          gcc_jit_function *func = gcc_jit_block_get_function(current_block);
                          gcc_jit_block *then_block = gcc_jit_function_new_block(func, "ternary_then");
                          gcc_jit_block *else_block = gcc_jit_function_new_block(func, "ternary_else");
                          gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "ternary_merge");
                          
                          gcc_jit_lvalue *res = gcc_jit_function_new_local(func, NULL, type, "ternary_res");
                          
                          gcc_jit_block_end_with_conditional(current_block, NULL, cond, then_block, else_block);
                          
                          current_block = then_block;
                          gcc_jit_rvalue *a = lower_expr_rvalue(op2);
                          if (a) gcc_jit_block_add_assignment(current_block, NULL, res, a);
                          gcc_jit_block_end_with_jump(current_block, NULL, merge_block);
                          
                          current_block = else_block;
                          gcc_jit_rvalue *b = lower_expr_rvalue(op3);
                          if (b) gcc_jit_block_add_assignment(current_block, NULL, res, b);
                          gcc_jit_block_end_with_jump(current_block, NULL, merge_block);
                          
                          current_block = merge_block;
                          return gcc_jit_lvalue_as_rvalue(res);
                      }
                  case eok_and:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_or:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_xor:
                      return gcc_jit_context_new_binary_op(gcc_jit_ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_XOR, type, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                      
                  case eok_eq:
                      return gcc_jit_context_new_comparison(gcc_jit_ctx, NULL, GCC_JIT_COMPARISON_EQ, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_ne:
                      return gcc_jit_context_new_comparison(gcc_jit_ctx, NULL, GCC_JIT_COMPARISON_NE, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_lt:
                      return gcc_jit_context_new_comparison(gcc_jit_ctx, NULL, GCC_JIT_COMPARISON_LT, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_le:
                      return gcc_jit_context_new_comparison(gcc_jit_ctx, NULL, GCC_JIT_COMPARISON_LE, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_gt:
                      return gcc_jit_context_new_comparison(gcc_jit_ctx, NULL, GCC_JIT_COMPARISON_GT, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  case eok_ge:
                      return gcc_jit_context_new_comparison(gcc_jit_ctx, NULL, GCC_JIT_COMPARISON_GE, lower_expr_rvalue(op1), lower_expr_rvalue(op2));
                  
                  case eok_negate:
                      return gcc_jit_context_new_unary_op(gcc_jit_ctx, NULL, GCC_JIT_UNARY_OP_MINUS, type, lower_expr_rvalue(op1));
                  case eok_complement:
                      return gcc_jit_context_new_unary_op(gcc_jit_ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, type, lower_expr_rvalue(op1));
                  case eok_not:
                      return gcc_jit_context_new_unary_op(gcc_jit_ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, lower_expr_rvalue(op1));
                      
                  case eok_address_of:
                      return gcc_jit_lvalue_get_address(lower_expr_lvalue(op1), NULL);
                      
                  case eok_indirect:
                      return gcc_jit_lvalue_as_rvalue(lower_expr_lvalue(expr));
                      
                  case eok_call:
                  case eok_dot_member_call:
                  case eok_points_to_member_call:
                      {
                          gcc_jit_function *func = NULL;
                          gcc_jit_rvalue *fn_ptr = NULL;
                          int num_args = 0;
                          an_expr_node_ptr arg = op2;
                          
                          if (op1->kind == enk_routine) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              func = (gcc_jit_function *)cache_lookup(func_cache, rout);
                              if (!func) func = lower_function_decl(rout);
                          } else {
                              fn_ptr = lower_expr_rvalue(op1);
                          }
                          
                          /* Count arguments */
                          while (arg) {
                              num_args++;
                              arg = arg->next;
                          }
                          
                          gcc_jit_rvalue **args = NULL;
                          if (num_args > 0) {
                              args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                              int i = 0;
                              arg = op2;
                              while (arg) {
                                  args[i++] = lower_expr_rvalue(arg);
                                  arg = arg->next;
                              }
                          }
                          
                          gcc_jit_rvalue *call_res = NULL;
                          if (func) {
                              call_res = gcc_jit_context_new_call(gcc_jit_ctx, NULL, func, num_args, args);
                          } else if (fn_ptr) {
                              call_res = gcc_jit_context_new_call_through_ptr(gcc_jit_ctx, NULL, fn_ptr, num_args, args);
                          } else {
                              call_res = gcc_jit_context_zero(gcc_jit_ctx, type);
                          }
                          
                          if (args) free(args);
                          return call_res;
                      }
                      
                  case eok_va_start:
                  case eok_va_start_single_operand:
                  case eok_va_arg:
                  case eok_va_end:
                  case eok_va_copy:
                      {
                          const char *bname = NULL;
                          if (op == eok_va_start || op == eok_va_start_single_operand) {
                              bname = "__builtin_va_start";
                          } else if (op == eok_va_arg) {
                              bname = "__builtin_va_arg";
                          } else if (op == eok_va_end) {
                              bname = "__builtin_va_end";
                          } else if (op == eok_va_copy) {
                              bname = "__builtin_va_copy";
                          }
                          
                          gcc_jit_function *bfunc = gcc_jit_context_get_builtin_function(gcc_jit_ctx, bname);
                          if (bfunc) {
                              int num_args = 0;
                              an_expr_node_ptr arg = op1;
                              while (arg) {
                                  num_args++;
                                  arg = arg->next;
                              }
                              
                              gcc_jit_rvalue **args = NULL;
                              if (num_args > 0) {
                                  args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                                  int i = 0;
                                  arg = op1;
                                  while (arg) {
                                      args[i++] = lower_expr_rvalue(arg);
                                      arg = arg->next;
                                  }
                              }
                              
                              gcc_jit_rvalue *call_res = gcc_jit_context_new_call(gcc_jit_ctx, NULL, bfunc, num_args, args);
                              if (args) free(args);
                              return call_res;
                          }
                          return gcc_jit_context_zero(gcc_jit_ctx, type);
                      }
                      
                  case eok_cast:
                      return gcc_jit_context_new_cast(gcc_jit_ctx, NULL, lower_expr_rvalue(op1), type);
                      
                  default:
                      /* Unhandled operators return a dummy zero for now */
                      return gcc_jit_context_zero(gcc_jit_ctx, type);
              }
          }
          
      case enk_routine:
          {
              a_routine_ptr rout = expr->variant.routine.ptr;
              gcc_jit_function *f = (gcc_jit_function *)cache_lookup(func_cache, rout);
              if (!f) {
                  f = lower_function_decl(rout);
              }
              if (f) return gcc_jit_function_get_address(f, NULL);
              return NULL;
          }
          
      case enk_throw:
          /* Stub: throw expression */
          return gcc_jit_context_zero(gcc_jit_ctx, lower_type(expr->type));
          
      default:
          return gcc_jit_context_zero(gcc_jit_ctx, lower_type(expr->type));
  }
}

static void lower_statement(a_statement_ptr stmt, gcc_jit_function *func) {
  while (stmt) {
      if (!current_block) {
          if (stmt->kind == stmk_label) {
              current_block = get_label_block(func, stmt->variant.label.ptr);
          } else if (stmt->kind == stmk_switch_case) {
              current_block = get_switch_case_block(func, stmt->variant.switch_case.extra_info);
          } else {
              stmt = stmt->next;
              continue;
          }
      } else {
          if (stmt->kind == stmk_label) {
              gcc_jit_block *label_block = get_label_block(func, stmt->variant.label.ptr);
              gcc_jit_block_end_with_jump(current_block, NULL, label_block);
              current_block = label_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = get_switch_case_block(func, stmt->variant.switch_case.extra_info);
              gcc_jit_block_end_with_jump(current_block, NULL, case_block);
              current_block = case_block;
          }
      }

      switch (stmt->kind) {
          case stmk_block:
              if (stmt->variant.block.statements) {
                  lower_statement(stmt->variant.block.statements, func);
              }
              break;

          case stmk_expr:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = lower_expr_rvalue(stmt->expr);
                  if (rval) gcc_jit_block_add_eval(current_block, NULL, rval);
              }
              break;

          case stmk_return:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = lower_expr_rvalue(stmt->expr);
                  if (!rval) rval = gcc_jit_context_zero(gcc_jit_ctx, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_block_end_with_return(current_block, NULL, rval);
              } else {
                  gcc_jit_block_end_with_void_return(current_block, NULL);
              }
              current_block = NULL;
              break;

          case stmk_goto:
              {
                  gcc_jit_block *target = get_label_block(func, stmt->variant.label.ptr);
                  gcc_jit_block_end_with_jump(current_block, NULL, target);
                  current_block = NULL;
              }
              break;

          case stmk_if:
          case stmk_constexpr_if:
          case stmk_if_consteval:
          case stmk_if_not_consteval:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) cond = lower_expr_rvalue(stmt->expr);
                  if (!cond) cond = gcc_jit_context_zero(gcc_jit_ctx, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT));

                  gcc_jit_block *then_block = gcc_jit_function_new_block(func, "if_then");
                  gcc_jit_block *else_block = gcc_jit_function_new_block(func, "if_else");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "if_merge");

                  gcc_jit_block_end_with_conditional(current_block, NULL, cond, then_block, else_block);

                  gcc_jit_block *after_then = NULL;
                  gcc_jit_block *after_else = NULL;

                  if (stmt->kind == stmk_constexpr_if) {
                      if (stmt->variant.constexpr_if->then_statement) {
                          current_block = then_block; lower_statement(stmt->variant.constexpr_if->then_statement, func); after_then = current_block;
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.constexpr_if->else_statement) {
                          current_block = else_block; lower_statement(stmt->variant.constexpr_if->else_statement, func); after_else = current_block;
                      }
                      else
                          after_else = else_block;
                  } else {
                      if (stmt->variant.if_stmt.then_statement) {
                          current_block = then_block; lower_statement(stmt->variant.if_stmt.then_statement, func); after_then = current_block;
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.if_stmt.else_statement) {
                          current_block = else_block; lower_statement(stmt->variant.if_stmt.else_statement, func); after_else = current_block;
                      }
                      else
                          after_else = else_block;
                  }

                  if (after_then) gcc_jit_block_end_with_jump(after_then, NULL, merge_block);
                  if (after_else) gcc_jit_block_end_with_jump(after_else, NULL, merge_block);

                  current_block = merge_block;
              }
              break;

          case stmk_switch:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) cond = lower_expr_rvalue(stmt->expr);
                  if (!cond) cond = gcc_jit_context_zero(gcc_jit_ctx, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT));

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
                  }

                  int i = 0;
                  scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) {
                          long val_int = 0;
                          if (scep->case_value && scep->case_value->kind == ck_integer) {
                              a_boolean err = FALSE;
                              val_int = (long)value_of_integer_constant(scep->case_value, &err);
                          }
                          gcc_jit_rvalue *val = gcc_jit_context_new_rvalue_from_long(gcc_jit_ctx, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_LONG), val_int);
                          /* Cast to condition type */
                          val = gcc_jit_context_new_cast(gcc_jit_ctx, NULL, val, gcc_jit_rvalue_get_type(cond));
                          gcc_jit_block *case_block = get_switch_case_block(func, scep);
                          cases[i++] = gcc_jit_context_new_case(gcc_jit_ctx, val, val, case_block);
                      }
                      scep = scep->next;
                  }

                  gcc_jit_block *default_block = NULL;
                  if (ssdp->default_case) {
                      default_block = get_switch_case_block(func, ssdp->default_case);
                  } else {
                      default_block = gcc_jit_function_new_block(func, "switch_default");
                  }

                  gcc_jit_block_end_with_switch(current_block, NULL, cond, default_block, num_cases, cases);
                  if (num_cases > 0) free(cases);

                  current_block = NULL; lower_statement(stmt->variant.switch_stmt.body_statement, func); gcc_jit_block *after_switch = current_block;
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "switch_exit");

                  if (!ssdp->default_case) {
                      gcc_jit_block_end_with_jump(default_block, NULL, exit_block);
                  }
                  if (after_switch) {
                      gcc_jit_block_end_with_jump(after_switch, NULL, exit_block);
                  }

                  current_block = exit_block;
              }
              break;

          case stmk_while:
          case stmk_end_test_while:
              {
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "loop_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "loop_body");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "loop_exit");

                  if (stmt->kind == stmk_end_test_while) {
                      gcc_jit_block_end_with_jump(current_block, NULL, body_block);
                  } else {
                      gcc_jit_block_end_with_jump(current_block, NULL, cond_block);
                  }

                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) cond = lower_expr_rvalue(stmt->expr);
                  if (!cond) cond = gcc_jit_context_one(gcc_jit_ctx, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT));

                  gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  current_block = body_block; lower_statement(stmt->variant.loop_statement, func); gcc_jit_block *after_body = current_block;
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, cond_block);

                  current_block = exit_block;
              }
              break;

          case stmk_for:
              {
                  a_for_loop_ptr loop_info = stmt->variant.for_loop.extra_info;
                  if (loop_info && loop_info->initialization) {
                      lower_statement(loop_info->initialization, func);
                  }
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "for_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "for_body");
                  gcc_jit_block *step_block = gcc_jit_function_new_block(func, "for_step");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "for_exit");

                  gcc_jit_block_end_with_jump(current_block, NULL, cond_block);

                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) cond = lower_expr_rvalue(stmt->expr);
                  if (!cond) cond = gcc_jit_context_one(gcc_jit_ctx, gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_INT));

                  gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  current_block = body_block; lower_statement(stmt->variant.for_loop.statement, func); gcc_jit_block *after_body = current_block;
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, step_block);

                  if (loop_info && loop_info->increment) {
                      gcc_jit_rvalue *rval = lower_expr_rvalue(loop_info->increment);
                      if (rval) gcc_jit_block_add_eval(step_block, NULL, rval);
                  }
                  gcc_jit_block_end_with_jump(step_block, NULL, cond_block);

                  current_block = exit_block;
              }
              break;

          case stmk_try_block:
              /* Stub: C++ try/catch */
              if (stmt->variant.try_block && stmt->variant.try_block->statement) {
                  lower_statement(stmt->variant.try_block->statement, func);
              }
              break;

          case stmk_decl:
              /* Ignored: variables allocated at function entry */
              break;
              
          case stmk_init:
              {
                  a_dynamic_init_ptr dip = stmt->variant.dynamic_init;
                  if (dip && dip->kind != dik_none && dip->variable) {
                      gcc_jit_lvalue *var_lvalue = (gcc_jit_lvalue *)cache_lookup(var_cache, dip->variable);
                      if (var_lvalue) {
                          gcc_jit_rvalue *rval = NULL;
                          if (dip->kind == dik_zero) {
                              rval = gcc_jit_context_zero(gcc_jit_ctx, lower_type(dip->variable->type));
                          } else if (dip->kind == dik_constant || dip->kind == dik_nonconstant_aggregate) {
                              a_constant_ptr con = dip->variant.constant.ptr;
                              gcc_jit_type *type = lower_type(dip->variable->type);
                              if (con->kind == ck_integer) {
                                  a_boolean err = FALSE;
                                  long val = (long)value_of_integer_constant(con, &err);
                                  rval = gcc_jit_context_new_rvalue_from_long(gcc_jit_ctx, type, val);
                              } else if (con->kind == ck_float) {
                                  double val = (double)fetch_host_fp_value(dip->variable->type->variant.float_kind, &con->variant.float_value);
                                  rval = gcc_jit_context_new_rvalue_from_double(gcc_jit_ctx, type, val);
                              } else if (con->kind == ck_string) {
                                  rval = gcc_jit_context_new_string_literal(gcc_jit_ctx, (const char *)con->variant.string.value);
                              } else {
                                  rval = gcc_jit_context_zero(gcc_jit_ctx, type);
                              }
                          } else if (dip->kind == dik_expression || dip->kind == dik_class_result_via_ctor) {
                              rval = lower_expr_rvalue(dip->variant.expression);
                          }
                          
                          if (rval) {
                              gcc_jit_block_add_assignment(current_block, NULL, var_lvalue, rval);
                          }
                      }
                  }
              }
              break;

          case stmk_asm:
              {
                  an_asm_entry_ptr asm_entry = stmt->variant.asm_entry;
                  if (asm_entry) {
                      const char *asm_str = "";
                      if (asm_entry->asm_string && asm_entry->asm_string->kind == ck_string) {
                          asm_str = (const char *)asm_entry->asm_string->variant.string.value;
                      }
                      
                      gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
                      if (ext_asm) {
                          gcc_jit_extended_asm_set_volatile_flag(ext_asm, asm_entry->is_volatile);
                          
                          an_asm_operand_ptr op = asm_entry->operands;
                          while (op) {
                              const char *name = op->name;
                              const char *constraint = op->constraints_string;
                              if (op->is_output_operand) {
                                  gcc_jit_lvalue *lval = lower_expr_lvalue(op->expression);
                                  if (lval) {
                                      gcc_jit_extended_asm_add_output_operand(ext_asm, name, constraint, lval);
                                  }
                              } else {
                                  gcc_jit_rvalue *rval = lower_expr_rvalue(op->expression);
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
  }

static gcc_jit_function *lower_function_decl(a_routine_ptr rout) {
  if (!rout) return NULL;
  
  gcc_jit_function *cached = (gcc_jit_function *)cache_lookup(func_cache, rout);
  if (cached) return cached;
  
  gcc_jit_type *ret_type = NULL;
  if (rout->type && rout->type->kind == tk_routine) {
    ret_type = lower_type(rout->type->variant.routine.return_type);
  } else {
    ret_type = gcc_jit_context_get_type(gcc_jit_ctx, GCC_JIT_TYPE_VOID);
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
  a_param_type_ptr ptp;
  if (rout->type && rout->type->kind == tk_routine) {
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          num_params++;
      }
  }
  
  gcc_jit_param **params = NULL;
  if (num_params > 0) {
      params = (gcc_jit_param **)malloc((size_t)num_params * sizeof(gcc_jit_param *));
      int i = 0;
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = scope_for_routine(rout);
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          gcc_jit_type *param_type = lower_type(ptp->type);
          const char *param_name = "unnamed_param";
          if (var_param) {
              if (var_param->source_corresp.name) param_name = var_param->source_corresp.name;
              var_param = var_param->next;
          }
          params[i] = gcc_jit_context_new_param(gcc_jit_ctx, NULL, param_type, param_name);
          i++;
      }
  }

  int is_variadic = 0;
  if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info->has_ellipsis) {
      is_variadic = 1;
  }

  gcc_jit_function *func = gcc_jit_context_new_function(
      gcc_jit_ctx, get_location(&rout->source_corresp.decl_position), linkage, ret_type, name, num_params, params, is_variadic);
      
  if (num_params > 0) {
      int i = 0;
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = scope_for_routine(rout);
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          if (var_param) {
              cache_insert(var_cache, var_param, gcc_jit_param_as_lvalue(params[i]));
              var_param = var_param->next;
          }
          i++;
      }
      free(params);
  }
  
  cache_insert(func_cache, rout, func);
  return func;
}

void gcc_gen_be(void) {
  a_scope_ptr scope;
  a_routine_ptr rout;
  a_variable_ptr var;

  gcc_gen_be_init();
  if (!gcc_jit_ctx) return;

  scope = il_header.primary_scope;
  
  if (scope) {
    for (var = scope->variables; var != NULL; var = var->next) {
        lower_variable_decl(var);
    }
    
    for (rout = scope->routines; rout != NULL; rout = rout->next) {
      if (ignore_routine_in_back_end(rout)) continue;

      gcc_jit_function *func = lower_function_decl(rout);

      if (rout->function_def_number != NULL_function_def_number) {
        a_scope_ptr def_scope = scope_for_routine(rout);
        if (def_scope && def_scope->assoc_block) {
          /* First, iterate and lower all block-local variables in the function */
          for (var = def_scope->variables; var != NULL; var = var->next) {
             /* Parameters are already handled in lower_function_decl */
             if (!var->is_parameter) {
                 const char *lname = var->source_corresp.name ? var->source_corresp.name : "unnamed_local";
                 gcc_jit_lvalue *local = gcc_jit_function_new_local(func, NULL, lower_type(var->type), lname);
                 cache_insert(var_cache, var, local);
             }
          }

          gcc_jit_block *block = gcc_jit_function_new_block(func, "entry");
          current_block = block; lower_statement(def_scope->assoc_block, func);
        }
      }
    }
  }

  
  if (primary_source_file_name) {
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
        }
    }
    
    gcc_jit_context_compile_to_file(gcc_jit_ctx, output_kind, obj_name);
    
    const char *err = gcc_jit_context_get_first_error(gcc_jit_ctx);
    if (err) {
        fprintf(f_error, "gcc_gen_be compilation error: %s\n", err);
        (void)0;
    }
  }

  gcc_gen_be_cleanup();
}

void back_end(void) {
  gcc_gen_be();
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
