/**
 * @file gcc_gen_be_lib_loader.h
 * @brief Dynamic loading for libgccjit.
 *
 * This file handles dynamically loading libgccjit on Windows, and provides
 * no-op or basic dlopen implementations for POSIX systems where it is usually
 * linked dynamically at build time.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_LIB_LOADER_H
#define GCC_GEN_BE_LIB_LOADER_H

#include "gcc_gen_be_error.h"

#if defined(_WIN32)
extern void *p_gcc_jit_context_acquire;
#define gcc_jit_context_acquire (...) ((__typeof__(gcc_jit_context_acquire) *)p_gcc_jit_context_acquire)(__VA_ARGS__)
extern void *p_gcc_jit_context_release;
#define gcc_jit_context_release (...) ((__typeof__(gcc_jit_context_release) *)p_gcc_jit_context_release)(__VA_ARGS__)
extern void *p_gcc_jit_context_set_int_option;
#define gcc_jit_context_set_int_option (...) ((__typeof__(gcc_jit_context_set_int_option) *)p_gcc_jit_context_set_int_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_set_bool_option;
#define gcc_jit_context_set_bool_option (...) ((__typeof__(gcc_jit_context_set_bool_option) *)p_gcc_jit_context_set_bool_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_set_str_option;
#define gcc_jit_context_set_str_option (...) ((__typeof__(gcc_jit_context_set_str_option) *)p_gcc_jit_context_set_str_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_add_command_line_option;
#define gcc_jit_context_add_command_line_option (...) ((__typeof__(gcc_jit_context_add_command_line_option) *)p_gcc_jit_context_add_command_line_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_get_type;
#define gcc_jit_context_get_type (...) ((__typeof__(gcc_jit_context_get_type) *)p_gcc_jit_context_get_type)(__VA_ARGS__)
extern void *p_gcc_jit_type_get_pointer;
#define gcc_jit_type_get_pointer (...) ((__typeof__(gcc_jit_type_get_pointer) *)p_gcc_jit_type_get_pointer)(__VA_ARGS__)
extern void *p_gcc_jit_type_get_const;
#define gcc_jit_type_get_const (...) ((__typeof__(gcc_jit_type_get_const) *)p_gcc_jit_type_get_const)(__VA_ARGS__)
extern void *p_gcc_jit_type_get_volatile;
#define gcc_jit_type_get_volatile (...) ((__typeof__(gcc_jit_type_get_volatile) *)p_gcc_jit_type_get_volatile)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_array_type;
#define gcc_jit_context_new_array_type (...) ((__typeof__(gcc_jit_context_new_array_type) *)p_gcc_jit_context_new_array_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_field;
#define gcc_jit_context_new_field (...) ((__typeof__(gcc_jit_context_new_field) *)p_gcc_jit_context_new_field)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_bitfield;
#define gcc_jit_context_new_bitfield (...) ((__typeof__(gcc_jit_context_new_bitfield) *)p_gcc_jit_context_new_bitfield)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_struct_type;
#define gcc_jit_context_new_struct_type (...) ((__typeof__(gcc_jit_context_new_struct_type) *)p_gcc_jit_context_new_struct_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_opaque_struct;
#define gcc_jit_context_new_opaque_struct (...) ((__typeof__(gcc_jit_context_new_opaque_struct) *)p_gcc_jit_context_new_opaque_struct)(__VA_ARGS__)
extern void *p_gcc_jit_struct_set_fields;
#define gcc_jit_struct_set_fields (...) ((__typeof__(gcc_jit_struct_set_fields) *)p_gcc_jit_struct_set_fields)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_union_type;
#define gcc_jit_context_new_union_type (...) ((__typeof__(gcc_jit_context_new_union_type) *)p_gcc_jit_context_new_union_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_function_ptr_type;
#define gcc_jit_context_new_function_ptr_type (...) ((__typeof__(gcc_jit_context_new_function_ptr_type) *)p_gcc_jit_context_new_function_ptr_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_param;
#define gcc_jit_context_new_param (...) ((__typeof__(gcc_jit_context_new_param) *)p_gcc_jit_context_new_param)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_function;
#define gcc_jit_context_new_function (...) ((__typeof__(gcc_jit_context_new_function) *)p_gcc_jit_context_new_function)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_global;
#define gcc_jit_context_new_global (...) ((__typeof__(gcc_jit_context_new_global) *)p_gcc_jit_context_new_global)(__VA_ARGS__)
extern void *p_gcc_jit_global_set_initializer;
#define gcc_jit_global_set_initializer (...) ((__typeof__(gcc_jit_global_set_initializer) *)p_gcc_jit_global_set_initializer)(__VA_ARGS__)
extern void *p_gcc_jit_function_new_block;
#define gcc_jit_function_new_block (...) ((__typeof__(gcc_jit_function_new_block) *)p_gcc_jit_function_new_block)(__VA_ARGS__)
extern void *p_gcc_jit_function_new_local;
#define gcc_jit_function_new_local (...) ((__typeof__(gcc_jit_function_new_local) *)p_gcc_jit_function_new_local)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_eval;
#define gcc_jit_block_add_eval (...) ((__typeof__(gcc_jit_block_add_eval) *)p_gcc_jit_block_add_eval)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_assignment;
#define gcc_jit_block_add_assignment (...) ((__typeof__(gcc_jit_block_add_assignment) *)p_gcc_jit_block_add_assignment)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_assignment_op;
#define gcc_jit_block_add_assignment_op (...) ((__typeof__(gcc_jit_block_add_assignment_op) *)p_gcc_jit_block_add_assignment_op)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_conditional;
#define gcc_jit_block_end_with_conditional (...) ((__typeof__(gcc_jit_block_end_with_conditional) *)p_gcc_jit_block_end_with_conditional)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_jump;
#define gcc_jit_block_end_with_jump (...) ((__typeof__(gcc_jit_block_end_with_jump) *)p_gcc_jit_block_end_with_jump)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_return;
#define gcc_jit_block_end_with_return (...) ((__typeof__(gcc_jit_block_end_with_return) *)p_gcc_jit_block_end_with_return)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_void_return;
#define gcc_jit_block_end_with_void_return (...) ((__typeof__(gcc_jit_block_end_with_void_return) *)p_gcc_jit_block_end_with_void_return)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_switch;
#define gcc_jit_block_end_with_switch (...) ((__typeof__(gcc_jit_block_end_with_switch) *)p_gcc_jit_block_end_with_switch)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_int;
#define gcc_jit_context_new_rvalue_from_int (...) ((__typeof__(gcc_jit_context_new_rvalue_from_int) *)p_gcc_jit_context_new_rvalue_from_int)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_long;
#define gcc_jit_context_new_rvalue_from_long (...) ((__typeof__(gcc_jit_context_new_rvalue_from_long) *)p_gcc_jit_context_new_rvalue_from_long)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_double;
#define gcc_jit_context_new_rvalue_from_double (...) ((__typeof__(gcc_jit_context_new_rvalue_from_double) *)p_gcc_jit_context_new_rvalue_from_double)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_ptr;
#define gcc_jit_context_new_rvalue_from_ptr (...) ((__typeof__(gcc_jit_context_new_rvalue_from_ptr) *)p_gcc_jit_context_new_rvalue_from_ptr)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_string_literal;
#define gcc_jit_context_new_string_literal (...) ((__typeof__(gcc_jit_context_new_string_literal) *)p_gcc_jit_context_new_string_literal)(__VA_ARGS__)
extern void *p_gcc_jit_context_null;
#define gcc_jit_context_null (...) ((__typeof__(gcc_jit_context_null) *)p_gcc_jit_context_null)(__VA_ARGS__)
extern void *p_gcc_jit_context_zero;
#define gcc_jit_context_zero (...) ((__typeof__(gcc_jit_context_zero) *)p_gcc_jit_context_zero)(__VA_ARGS__)
extern void *p_gcc_jit_context_one;
#define gcc_jit_context_one (...) ((__typeof__(gcc_jit_context_one) *)p_gcc_jit_context_one)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_unary_op;
#define gcc_jit_context_new_unary_op (...) ((__typeof__(gcc_jit_context_new_unary_op) *)p_gcc_jit_context_new_unary_op)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_binary_op;
#define gcc_jit_context_new_binary_op (...) ((__typeof__(gcc_jit_context_new_binary_op) *)p_gcc_jit_context_new_binary_op)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_comparison;
#define gcc_jit_context_new_comparison (...) ((__typeof__(gcc_jit_context_new_comparison) *)p_gcc_jit_context_new_comparison)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_cast;
#define gcc_jit_context_new_cast (...) ((__typeof__(gcc_jit_context_new_cast) *)p_gcc_jit_context_new_cast)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_array_access;
#define gcc_jit_context_new_array_access (...) ((__typeof__(gcc_jit_context_new_array_access) *)p_gcc_jit_context_new_array_access)(__VA_ARGS__)
extern void *p_gcc_jit_lvalue_access_field;
#define gcc_jit_lvalue_access_field (...) ((__typeof__(gcc_jit_lvalue_access_field) *)p_gcc_jit_lvalue_access_field)(__VA_ARGS__)
extern void *p_gcc_jit_rvalue_access_field;
#define gcc_jit_rvalue_access_field (...) ((__typeof__(gcc_jit_rvalue_access_field) *)p_gcc_jit_rvalue_access_field)(__VA_ARGS__)
extern void *p_gcc_jit_rvalue_dereference_field;
#define gcc_jit_rvalue_dereference_field (...) ((__typeof__(gcc_jit_rvalue_dereference_field) *)p_gcc_jit_rvalue_dereference_field)(__VA_ARGS__)
extern void *p_gcc_jit_rvalue_dereference;
#define gcc_jit_rvalue_dereference (...) ((__typeof__(gcc_jit_rvalue_dereference) *)p_gcc_jit_rvalue_dereference)(__VA_ARGS__)
extern void *p_gcc_jit_lvalue_get_address;
#define gcc_jit_lvalue_get_address (...) ((__typeof__(gcc_jit_lvalue_get_address) *)p_gcc_jit_lvalue_get_address)(__VA_ARGS__)
extern void *p_gcc_jit_lvalue_as_rvalue;
#define gcc_jit_lvalue_as_rvalue (...) ((__typeof__(gcc_jit_lvalue_as_rvalue) *)p_gcc_jit_lvalue_as_rvalue)(__VA_ARGS__)
extern void *p_gcc_jit_param_as_lvalue;
#define gcc_jit_param_as_lvalue (...) ((__typeof__(gcc_jit_param_as_lvalue) *)p_gcc_jit_param_as_lvalue)(__VA_ARGS__)
extern void *p_gcc_jit_param_as_rvalue;
#define gcc_jit_param_as_rvalue (...) ((__typeof__(gcc_jit_param_as_rvalue) *)p_gcc_jit_param_as_rvalue)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_call;
#define gcc_jit_context_new_call (...) ((__typeof__(gcc_jit_context_new_call) *)p_gcc_jit_context_new_call)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_call_through_ptr;
#define gcc_jit_context_new_call_through_ptr (...) ((__typeof__(gcc_jit_context_new_call_through_ptr) *)p_gcc_jit_context_new_call_through_ptr)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_extended_asm;
#define gcc_jit_block_add_extended_asm (...) ((__typeof__(gcc_jit_block_add_extended_asm) *)p_gcc_jit_block_add_extended_asm)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_location;
#define gcc_jit_context_new_location (...) ((__typeof__(gcc_jit_context_new_location) *)p_gcc_jit_context_new_location)(__VA_ARGS__)
extern void *p_gcc_jit_context_compile_to_file;
#define gcc_jit_context_compile_to_file (...) ((__typeof__(gcc_jit_context_compile_to_file) *)p_gcc_jit_context_compile_to_file)(__VA_ARGS__)
extern void *p_gcc_jit_context_get_first_error;
#define gcc_jit_context_get_first_error (...) ((__typeof__(gcc_jit_context_get_first_error) *)p_gcc_jit_context_get_first_error)(__VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads the libgccjit dynamic library on Windows.
 *
 * @return GCC_GEN_BE_SUCCESS if loaded successfully, or an error code otherwise.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t load_libgccjit_windows(void);

/**
 * @brief Loads the libgccjit dynamic library on POSIX systems.
 *
 * @return GCC_GEN_BE_SUCCESS if loaded successfully, or an error code otherwise.
 */
extern GCC_GEN_BE_NODISCARD gcc_gen_be_error_t load_libgccjit_posix(void);

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_LIB_LOADER_H */
