/**
 * @file gcc_gen_be_expr.c
 * @brief Implementation of expression lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!expr) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  gcc_gen_be_error_t err;

  switch (expr->kind) {
      case enk_variable:
          {
              a_variable_ptr var = expr->variant.variable.ptr;
              void *cached = NULL;
              err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              if (!cached) {
                  gcc_jit_lvalue *lval = NULL;
                  err = gcc_gen_be_lower_global_variable_decl(var, &lval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  *out_lval = lval;
              } else {
                  *out_lval = (gcc_jit_lvalue *)cached;
              }
              return GCC_GEN_BE_SUCCESS;
          }
          
      case enk_operation:
          {
              an_expr_operator_kind op = expr->variant.operation.kind;
              an_expr_node_ptr op1 = expr->variant.operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;
              
              if (op == eok_indirect || op == eok_ref_indirect) {
                  gcc_jit_rvalue *ptr = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  *out_lval = gcc_jit_rvalue_dereference(ptr, NULL);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_dot_field || op == eok_points_to_field) {
                  a_field_ptr f = expr->variant.operation.operands->next->variant.field.ptr;
                  void *cached = NULL;
                  err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached);
                  
                  /* If field is not cached, it might be because the struct type hasn't been completely lowered in this scope yet, 
                     so force lowering the struct type of op1 */
                  if (err != GCC_GEN_BE_SUCCESS || !cached) {
                      gcc_jit_type *dummy = NULL;
                      err = gcc_gen_be_lower_type(op1->type, &dummy);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached);
                      if (err != GCC_GEN_BE_SUCCESS || !cached) return GCC_GEN_BE_ERROR_INTERNAL;
                  }
                  
                  gcc_jit_field *field = (gcc_jit_field *)cached;
                  
                  if (op == eok_dot_field) {
                      gcc_jit_lvalue *base = NULL;
                      err = gcc_gen_be_lower_expr_lvalue(op1, &base);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      *out_lval = gcc_jit_lvalue_access_field(base, NULL, field);
                  } else {
                      gcc_jit_rvalue *ptr = NULL;
                      err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      *out_lval = gcc_jit_rvalue_dereference_field(ptr, NULL, field);
                  }
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_subscript) {
                  gcc_jit_rvalue *ptr = NULL;
                  gcc_jit_lvalue *arr_lval = NULL;
                  a_type_ptr op1_stripped = op1->type;
                  while (op1_stripped && op1_stripped->kind == tk_typeref) {
                      op1_stripped = op1_stripped->variant.typeref.type;
                  }
                  if (op1_stripped && op1_stripped->kind == tk_array) {
                      err = gcc_gen_be_lower_expr_lvalue(op1, &arr_lval);
                      if (err == GCC_GEN_BE_SUCCESS && arr_lval) {
                          gcc_jit_type *elem_type = NULL;
                          err = gcc_gen_be_lower_type(op1_stripped->variant.array.element_type, &elem_type);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                          ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(arr_lval, NULL), elem_ptr_type);
                      }
                  }
                  if (!ptr) {
                      err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  
                  gcc_jit_rvalue *idx = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &idx);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  *out_lval = gcc_jit_context_new_array_access(ctx, NULL, ptr, idx);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_pre_incr || op == eok_pre_decr) {
                  gcc_jit_lvalue *l1 = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_type *type = NULL;
                  err = gcc_gen_be_lower_type(op1->type, &type);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, type, 1);
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      gcc_jit_block_add_assignment_op(cblock, NULL, l1, 
                          op == eok_pre_incr ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS, one);
                  }
                  *out_lval = l1;
                  return GCC_GEN_BE_SUCCESS;
              }
              *out_lval = NULL;
              return GCC_GEN_BE_SUCCESS;
          }
          
      default:
          *out_lval = NULL;
          return GCC_GEN_BE_SUCCESS;
  }
    return GCC_GEN_BE_SUCCESS;
}



/**
 * @brief Safely casts or bitcasts an rvalue to a destination type.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] val The rvalue to cast.
 * @param[in] dest_type The target type.
 * @return The cast rvalue.
 */
static gcc_jit_rvalue *safe_ptr_or_int_cast(gcc_jit_context *ctx, gcc_jit_rvalue *val, gcc_jit_type *dest_type) {
    if (!val || !dest_type) return NULL;
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(val);
    if (src_type == dest_type) return val;
#if defined(LIBGCCJIT_HAVE_gcc_jit_context_new_bitcast)
    size_t s1 = gcc_jit_type_get_size(src_type);
    size_t s2 = gcc_jit_type_get_size(dest_type);
    if (s1 == s2 && s1 > 0) {
        return gcc_jit_context_new_bitcast(ctx, NULL, val, dest_type);
    }
#endif
    return gcc_jit_context_new_cast(ctx, NULL, val, dest_type);
}

/**
 * @brief Lowers an address constant.
 *
 * @details Processes a constant address expression into a gcc_jit_rvalue.
 *
 * @param[in] con The frontend constant.
 * @param[in] expected_type The expected gcc_jit_type.
 * @param[out] out_rval Pointer to receive the resulting rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t lower_address_constant(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    gcc_jit_rvalue *base_rval = NULL;
    if (con->variant.address.kind == abk_variable) {
        a_variable_ptr var = con->variant.address.variant.variable;
        void *cached = NULL;
        gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
        gcc_jit_lvalue *lval = NULL;
        if (err == GCC_GEN_BE_SUCCESS && cached) {
            lval = (gcc_jit_lvalue *)cached;
        } else if (var) {
            extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
            err = gcc_gen_be_lower_global_variable_decl(var, &lval);
            if (err != GCC_GEN_BE_SUCCESS) lval = NULL;
        }
        if (lval) {
            base_rval = gcc_jit_lvalue_get_address(lval, NULL);
        }
    } else if (con->variant.address.kind == abk_routine) {
        a_routine_ptr rout = con->variant.address.variant.routine;
        void *cached = NULL;
        gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
        gcc_jit_function *func = NULL;
        if (err == GCC_GEN_BE_SUCCESS && cached) {
            func = (gcc_jit_function *)cached;
        } else if (rout) {
            extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
            err = gcc_gen_be_lower_function_decl(rout, &func);
            if (err != GCC_GEN_BE_SUCCESS) func = NULL;
        }
        if (func) {
            base_rval = gcc_jit_function_get_address(func, NULL);
        }
    } else if (con->variant.address.kind == abk_constant) {
        gcc_gen_be_error_t lower_err = gcc_gen_be_lower_constant_rvalue(con->variant.address.variant.constant, expected_type, &base_rval);
        if (lower_err != GCC_GEN_BE_SUCCESS) return lower_err;
    }
    
    if (!base_rval) {
        return gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
    }

    if (con->variant.address.offset != 0) {
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        
        gcc_jit_rvalue *c1 = safe_ptr_or_int_cast(ctx, base_rval, long_type);
        gcc_jit_rvalue *c2 = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)con->variant.address.offset);
        gcc_jit_rvalue *add = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, long_type, c1, c2);
        
        *out_rval = safe_ptr_or_int_cast(ctx, add, expected_type);
    } else {
        *out_rval = safe_ptr_or_int_cast(ctx, base_rval, expected_type);
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an aggregate constant.
 *
 * @details Processes a constant aggregate into a gcc_jit_rvalue.
 *
 * @param[in] con The frontend constant.
 * @param[in] type The expected gcc_jit_type.
 * @param[out] out_rval Pointer to receive the resulting rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t lower_aggregate_constant(a_constant_ptr con, gcc_jit_type *type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (con->type && con->type->kind == tk_array) {
        size_t count = 0;
        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
            if (c->kind == ck_designator) c = c->next;
            count++;
        }
        
        gcc_jit_rvalue **vals = NULL;
        if (count > 0) {
            vals = (gcc_jit_rvalue **)malloc(count * sizeof(gcc_jit_rvalue *));
            size_t idx = 0;
            for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                if (c->kind == ck_designator) c = c->next;
                gcc_jit_type *elem_type = NULL;
                err = gcc_gen_be_lower_type(con->type->variant.array.element_type, &elem_type);
                if (err == GCC_GEN_BE_SUCCESS) {
                    (void)gcc_gen_be_lower_constant_rvalue(c, elem_type, &vals[idx]);
                } else {
                    vals[idx] = NULL;
                }
                idx++;
            }
        }
        *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, type, count, vals);
        if (vals) free(vals);
    } else {
        size_t count = 0;
        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
            if (c->kind == ck_designator) c = c->next;
            count++;
        }
        
        gcc_jit_field **fields = NULL;
        gcc_jit_rvalue **vals = NULL;
        if (count > 0) {
            fields = (gcc_jit_field **)malloc(count * sizeof(gcc_jit_field *));
            vals = (gcc_jit_rvalue **)malloc(count * sizeof(gcc_jit_rvalue *));
            size_t idx = 0;
            a_field_ptr f = NULL;
            if (con->type && (con->type->kind == tk_struct || con->type->kind == tk_union || con->type->kind == tk_class)) {
                f = con->type->variant.class_struct_union.field_list;
            }
            
            for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                a_constant_ptr val_c = c;
                if (c->kind == ck_designator) {
                    if (c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
                        f = c->variant.designator.variant.field;
                    }
                    c = c->next;
                    val_c = c;
                }
                
                gcc_jit_field *jit_f = NULL;
                if (f) {
                    void *cached = NULL;
                    if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached) == GCC_GEN_BE_SUCCESS && cached) {
                        jit_f = (gcc_jit_field *)cached;
                    }
                }
                fields[idx] = jit_f;
                
                gcc_jit_type *f_type = NULL;
                if (f) { err = gcc_gen_be_lower_type(f->type, &f_type); }
                if (!f_type && val_c && val_c->type) { err = gcc_gen_be_lower_type(val_c->type, &f_type); }
                if (!f_type) f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                
                err = gcc_gen_be_lower_constant_rvalue(val_c, f_type, &vals[idx]);
                idx++;
                
                if (f) f = f->next;
            }
        }
        
        if (con->type && con->type->kind == tk_union) {
            if (count > 0 && fields[0] && vals[0]) {
                *out_rval = gcc_jit_context_new_union_constructor(ctx, NULL, type, fields[0], vals[0]);
            } else {
                err = gcc_gen_be_get_zero_rvalue(con->type, type, out_rval);
            }
        } else if (con->type && (con->type->kind == tk_struct || con->type->kind == tk_class)) {
            gcc_jit_type *struct_type = NULL;
            if (con->type) { err = gcc_gen_be_lower_type(con->type, &struct_type); }
            if (err != GCC_GEN_BE_SUCCESS || !struct_type) struct_type = type;
            *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, count, fields, vals);
        } else {
            if (count == 1 && vals && vals[0]) {
                *out_rval = vals[0];
            } else {
                err = gcc_gen_be_get_zero_rvalue(con->type, type, out_rval);
            }
        }
        
        if (fields) free(fields);
        if (vals) free(vals);
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval || !con) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (con->kind == ck_integer) {
        a_boolean local_err = FALSE;
        long val = (long)value_of_integer_constant(con, &local_err);
        if (val == 0) {
            *out_rval = safe_ptr_or_int_cast(ctx, gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), 0), expected_type);
            if (!*out_rval) {
                (void)gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
            }
        } else {
            gcc_jit_rvalue *num = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), val);
            *out_rval = safe_ptr_or_int_cast(ctx, num, expected_type);
        }
    } else if (con->kind == ck_float) {
        double val = (double)fetch_host_fp_value(fk_double, &con->variant.float_value);
        *out_rval = gcc_jit_context_new_rvalue_from_double(ctx, expected_type, val);
    } else if (con->kind == ck_string) {
        if (con->type && con->type->kind == tk_array) {
            a_type_ptr elem_type_ptr = con->type->variant.array.element_type;
            gcc_jit_type *elem_type = NULL;
            err = gcc_gen_be_lower_type(elem_type_ptr, &elem_type);
            long array_elems = con->type->variant.array.variant.number_of_elements;
            
            size_t bytes_per_elem = 1;
            if (elem_type_ptr->kind == tk_integer) bytes_per_elem = size_of_type(elem_type_ptr);
            
            size_t num_values = array_elems > 0 ? (size_t)array_elems : ((size_t)con->variant.string.length / bytes_per_elem);
            gcc_jit_rvalue **vals = (gcc_jit_rvalue **)malloc(num_values * sizeof(gcc_jit_rvalue *));
            
            const unsigned char *bytes = (const unsigned char *)con->variant.string.value;
            size_t str_len_bytes = (size_t)con->variant.string.length;
            
            for (size_t i = 0; i < num_values; ++i) {
                long long val = 0;
                size_t offset = i * bytes_per_elem;
                if (offset < str_len_bytes) {
                    for (size_t b = 0; b < bytes_per_elem && (offset + b) < str_len_bytes; ++b) {
                        val |= ((long long)bytes[offset + b]) << (b * 8);
                    }
                }
                vals[i] = gcc_jit_context_new_rvalue_from_long(ctx, elem_type, (long)val);
            }
            
            *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, expected_type, num_values, vals);
            free(vals);
        } else {
            *out_rval = gcc_jit_context_new_string_literal(ctx, (const char *)con->variant.string.value);
        }
    } else if (con->kind == ck_aggregate) {
        err = lower_aggregate_constant(con, expected_type, out_rval);
        if (err != GCC_GEN_BE_SUCCESS) *out_rval = NULL;
    } else if (con->kind == ck_address) {
        err = lower_address_constant(con, expected_type, out_rval);
        if (err != GCC_GEN_BE_SUCCESS) *out_rval = NULL;
    } else {
        return gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
  if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_rval = NULL;
  if (!expr) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  gcc_gen_be_error_t err;

  switch (expr->kind) {
      case enk_constant:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              return gcc_gen_be_lower_constant_rvalue(expr->variant.constant.ptr, type, out_rval);
          }
          
      case enk_variable:
          {
              gcc_jit_lvalue *lval = NULL;
              err = gcc_gen_be_lower_expr_lvalue(expr, &lval);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (lval) {
                  a_type_ptr etype = expr->type;
                  while (etype && etype->kind == tk_typeref) etype = etype->variant.typeref.type;
                  if (etype && etype->kind == tk_array) {
                      gcc_jit_type *elem_type = NULL;
                      err = gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                      *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
                  } else {
                      *out_rval = gcc_jit_lvalue_as_rvalue(lval);
                  }
              }
              return GCC_GEN_BE_SUCCESS;
          }
          
      case enk_operation:
          {
              an_expr_operator_kind op = expr->variant.operation.kind;
              an_expr_node_ptr op1 = expr->variant.operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;
              
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;

              if (op == eok_dot_field || op == eok_points_to_field || op == eok_subscript || op == eok_indirect || op == eok_ref_indirect) {
                  gcc_jit_lvalue *lval = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(expr, &lval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  if (lval) {
                      a_type_ptr etype = expr->type;
                      while (etype && etype->kind == tk_typeref) etype = etype->variant.typeref.type;
                      if (etype && etype->kind == tk_array) {
                          gcc_jit_type *elem_type = NULL;
                          err = gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                          *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
                      } else {
                          *out_rval = gcc_jit_lvalue_as_rvalue(lval);
                      }
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              
              if (op == eok_land) {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_lvalue *res = gcc_jit_function_new_local(func, NULL, bool_type, "land_res");
                  
                  gcc_jit_block *rhs_block = gcc_jit_function_new_block(func, "land_rhs");
                  gcc_jit_block *false_block = gcc_jit_function_new_block(func, "land_false");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "land_merge");
                  
                  gcc_jit_rvalue *r1 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_block_end_with_conditional(cblock, NULL, gcc_jit_context_new_cast(ctx, NULL, r1, bool_type), rhs_block, false_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(false_block));
                  gcc_jit_block_add_assignment(false_block, NULL, res, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 0));
                  gcc_jit_block_end_with_jump(false_block, NULL, merge_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rhs_block));
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&rhs_block));
                  if (rhs_block) {
                      gcc_jit_block_add_assignment(rhs_block, NULL, res, gcc_jit_context_new_cast(ctx, NULL, r2, bool_type));
                      gcc_jit_block_end_with_jump(rhs_block, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(res), type);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_lor) {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_lvalue *res = gcc_jit_function_new_local(func, NULL, bool_type, "lor_res");
                  
                  gcc_jit_block *true_block = gcc_jit_function_new_block(func, "lor_true");
                  gcc_jit_block *rhs_block = gcc_jit_function_new_block(func, "lor_rhs");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "lor_merge");
                  
                  gcc_jit_rvalue *r1 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_block_end_with_conditional(cblock, NULL, gcc_jit_context_new_cast(ctx, NULL, r1, bool_type), true_block, rhs_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(true_block));
                  gcc_jit_block_add_assignment(true_block, NULL, res, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 1));
                  gcc_jit_block_end_with_jump(true_block, NULL, merge_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rhs_block));
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&rhs_block));
                  if (rhs_block) {
                      gcc_jit_block_add_assignment(rhs_block, NULL, res, gcc_jit_context_new_cast(ctx, NULL, r2, bool_type));
                      gcc_jit_block_end_with_jump(rhs_block, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(res), type);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_question) {
                  an_expr_node_ptr op3 = op2 ? op2->next : NULL;
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  gcc_jit_block *then_block = gcc_jit_function_new_block(func, "ternary_then");
                  gcc_jit_block *else_block = gcc_jit_function_new_block(func, "ternary_else");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "ternary_merge");
                  
                  gcc_jit_lvalue *res = gcc_jit_function_new_local(func, NULL, type, "ternary_res");
                  
                  gcc_jit_rvalue *r1 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_block_end_with_conditional(cblock, NULL, gcc_jit_context_new_cast(ctx, NULL, r1, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)), then_block, else_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block));
                  gcc_jit_rvalue *r2 = NULL;
                  if (op2) { err = gcc_gen_be_lower_expr_rvalue(op2, &r2); if (err != GCC_GEN_BE_SUCCESS) return err; }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&then_block));
                  if (then_block) {
                      if (r2) gcc_jit_block_add_assignment(then_block, NULL, res, r2);
                      gcc_jit_block_end_with_jump(then_block, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block));
                  gcc_jit_rvalue *r3 = NULL;
                  if (op3) { err = gcc_gen_be_lower_expr_rvalue(op3, &r3); if (err != GCC_GEN_BE_SUCCESS) return err; }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&else_block));
                  if (else_block) {
                      if (r3) gcc_jit_block_add_assignment(else_block, NULL, res, r3);
                      gcc_jit_block_end_with_jump(else_block, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = gcc_jit_lvalue_as_rvalue(res);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_assign || op == eok_bassign || op == eok_add_assign || op == eok_subtract_assign || op == eok_multiply_assign || op == eok_divide_assign || op == eok_remainder_assign || op == eok_shiftl_assign || op == eok_shiftr_assign || op == eok_and_assign || op == eok_or_assign || op == eok_xor_assign || op == eok_padd_assign || op == eok_psubtract_assign) {
                  gcc_jit_lvalue *l1 = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      if (op == eok_assign || op == eok_bassign) {
                          gcc_jit_block_add_assignment(cblock, NULL, l1, r2);
                      } else if (op == eok_padd_assign) {
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(l1), r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, NULL, l1, new_ptr);
                      } else if (op == eok_psubtract_assign) {
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_MINUS, gcc_jit_rvalue_get_type(r2), r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(l1), neg_r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, NULL, l1, new_ptr);
                      } else {
                          enum gcc_jit_binary_op jit_op;
                          switch (op) {
                              case eok_add_assign: jit_op = GCC_JIT_BINARY_OP_PLUS; break;
                              case eok_subtract_assign: jit_op = GCC_JIT_BINARY_OP_MINUS; break;
                              case eok_multiply_assign: jit_op = GCC_JIT_BINARY_OP_MULT; break;
                              case eok_divide_assign: jit_op = GCC_JIT_BINARY_OP_DIVIDE; break;
                              case eok_remainder_assign: jit_op = GCC_JIT_BINARY_OP_MODULO; break;
                              case eok_shiftl_assign: jit_op = GCC_JIT_BINARY_OP_LSHIFT; break;
                              case eok_shiftr_assign: jit_op = GCC_JIT_BINARY_OP_RSHIFT; break;
                              case eok_and_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_AND; break;
                              case eok_or_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_OR; break;
                              case eok_xor_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_XOR; break;
                              default: return GCC_GEN_BE_ERROR_INTERNAL;
                          }
                          gcc_jit_block_add_assignment_op(cblock, NULL, l1, jit_op, r2);
                      }
                  }
                  *out_rval = gcc_jit_lvalue_as_rvalue(l1);
                  return GCC_GEN_BE_SUCCESS;
              }
              
              gcc_jit_rvalue *r1 = NULL;
              if (op1) { err = gcc_gen_be_lower_expr_rvalue(op1, &r1); if (err != GCC_GEN_BE_SUCCESS) return err; }
              
              gcc_jit_rvalue *r2 = NULL;
              if (op2) { err = gcc_gen_be_lower_expr_rvalue(op2, &r2); if (err != GCC_GEN_BE_SUCCESS) return err; }
              
              switch (op) {
                  case eok_dynamic_cast:
                      err = gcc_gen_be_lower_dynamic_cast(expr, out_rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      return GCC_GEN_BE_SUCCESS;
                  case eok_padd:
                      {
                          a_type_ptr op1_t = op1 ? op1->type : NULL;
                          while (op1_t && op1_t->kind == tk_typeref) op1_t = op1_t->variant.typeref.type;
                          a_boolean op1_is_ptr = op1_t && (op1_t->kind == tk_pointer || op1_t->kind == tk_array);
                          gcc_jit_rvalue *ptr_val = op1_is_ptr ? r1 : r2;
                          gcc_jit_rvalue *int_val = op1_is_ptr ? r2 : r1;
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, NULL, ptr_val, int_val);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_psubtract:
                      {
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_MINUS, gcc_jit_rvalue_get_type(r2), r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, NULL, r1, neg_r2);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_pdiff:
                      {
                          gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *c1 = gcc_jit_context_new_cast(ctx, NULL, r1, long_type);
                          gcc_jit_rvalue *c2 = gcc_jit_context_new_cast(ctx, NULL, r2, long_type);
                          gcc_jit_rvalue *diff = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, long_type, c1, c2);
                          
                          a_type_ptr elem_type = op1->type->variant.pointer.type;
                          long sz = (long)size_of_type(elem_type);
                          gcc_jit_rvalue *sz_val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, sz);
                          
                          *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, long_type, diff, sz_val);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_spaceship:
                      {
                          gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                          gcc_jit_rvalue *cmp_lt = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, r1, r2);
                          gcc_jit_rvalue *cmp_gt = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GT, r1, r2);
                          
                          gcc_jit_rvalue *lt_val = gcc_jit_context_new_cast(ctx, NULL, cmp_lt, int_type);
                          gcc_jit_rvalue *gt_val = gcc_jit_context_new_cast(ctx, NULL, cmp_gt, int_type);
                          
                          *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, int_type, gt_val, lt_val);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_add: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_subtract: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_multiply: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_divide: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_remainder: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MODULO, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_shiftl: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_shiftr: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_comma:
                      {
                          gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (r1 && cblock) gcc_jit_block_add_eval(cblock, NULL, r1);
                          *out_rval = r2;
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_and: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_or: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_xor: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_XOR, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                      
                  case eok_eq: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_ne: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_lt: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_le: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_gt: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_ge: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  
                  case eok_fjadd:
                  case eok_jfadd: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_fjsubtract:
                  case eok_jfsubtract: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, type, r1, r2); return GCC_GEN_BE_SUCCESS;

                  case eok_vector_eq: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_ne: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_lt: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_le: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_gt: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_ge: *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_land: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_AND, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_lor: *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_OR, type, r1, r2); return GCC_GEN_BE_SUCCESS;

                  case eok_vector_not: *out_rval = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_negate: *out_rval = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_MINUS, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_complement: *out_rval = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_not: *out_rval = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;

                  case eok_post_incr:
                  case eok_post_decr:
                      {
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          
                          gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                          
                          gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                          gcc_jit_lvalue *temp = gcc_jit_function_new_local(func, NULL, type, "post_inc_temp");
                          
                          gcc_jit_block_add_assignment(cblock, NULL, temp, gcc_jit_lvalue_as_rvalue(l1));
                          
                          gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, type, 1);
                          gcc_jit_block_add_assignment_op(cblock, NULL, l1, 
                              op == eok_post_incr ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS, one);
                              
                          *out_rval = gcc_jit_lvalue_as_rvalue(temp);
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_address_of:
                      {
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          *out_rval = gcc_jit_lvalue_get_address(l1, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_indirect:
                  case eok_ref_indirect:
                  case eok_dot_field:
                  case eok_points_to_field:
                  case eok_subscript:
                  case eok_pre_incr:
                  case eok_pre_decr:
                      {
                          gcc_jit_lvalue *lexpr = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(expr, &lexpr);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          if (lexpr) {
                              a_type_ptr etype = expr->type;
                              while (etype && etype->kind == tk_typeref) etype = etype->variant.typeref.type;
                              if (etype && etype->kind == tk_array) {
                                  gcc_jit_type *elem_type = NULL;
                                  err = gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(lexpr, NULL), elem_ptr_type);
                              } else {
                                  *out_rval = gcc_jit_lvalue_as_rvalue(lexpr);
                              }
                          } else {
                              *out_rval = NULL;
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_call:
                  case eok_dot_member_call:
                  case eok_points_to_member_call:
                      {
                          gcc_jit_function *func = NULL;
                          gcc_jit_rvalue *fn_ptr = NULL;
                          int num_args = 0;
                          
                          int is_member = (op == eok_dot_member_call || op == eok_points_to_member_call);
                          
                          if (op1->kind == enk_routine) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              void *cached = NULL;
                              err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              
                              if (!cached) {
                                  err = gcc_gen_be_lower_function_decl(rout, &func);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                              } else {
                                  func = (gcc_jit_function *)cached;
                              }
                          } else {
                              fn_ptr = r1;
                          }
                          
                          an_expr_node_ptr arg = op2;
                          while (arg) {
                              num_args++;
                              arg = arg->next;
                          }
                          
                          gcc_jit_rvalue **args = NULL;
                          if (num_args > 0) {
                              args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                              if (!args) return GCC_GEN_BE_ERROR_OOM;
                              
                              int i = 0;
                              arg = op2;
                              if (is_member) {
                                  gcc_jit_rvalue *this_ptr = NULL;
                                  if (op == eok_dot_member_call) {
                                      gcc_jit_lvalue *base_lval = NULL;
                                      err = gcc_gen_be_lower_expr_lvalue(arg, &base_lval);
                                      if (err == GCC_GEN_BE_SUCCESS && base_lval) {
                                          this_ptr = gcc_jit_lvalue_get_address(base_lval, NULL);
                                      } else {
                                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &this_ptr));
                                      }
                                  } else {
                                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &this_ptr));
                                  }
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  args[i++] = this_ptr;
                                  arg = arg->next;
                              }
                              
                              while (arg) {
                                  err = gcc_gen_be_lower_expr_rvalue(arg, &args[i]);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  i++;
                                  arg = arg->next;
                              }
                          }
                          
                          if (op1->kind == enk_routine && op1->variant.routine.ptr->variant.builtin_function_kind != bfk_none) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              err = gcc_gen_be_lower_builtin_call(rout, num_args, args, out_rval);
                              if (args) free(args);
                              return err;
                          }

                          if (func) {
                              int param_count = (int)gcc_jit_function_get_param_count(func);
                              for (int i = 0; i < num_args && i < param_count; ++i) {
                                  gcc_jit_param *param = gcc_jit_function_get_param(func, i);
                                  gcc_jit_type *param_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                                  gcc_jit_type *arg_type = gcc_jit_rvalue_get_type(args[i]);
                                  if (arg_type != param_type) {
                                      if (p_gcc_jit_context_new_bitcast) {
                                          args[i] = p_gcc_jit_context_new_bitcast(ctx, NULL, args[i], param_type);
                                      } else {
                                          gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                          gcc_jit_rvalue *vptr = gcc_jit_context_new_cast(ctx, NULL, args[i], void_ptr_type);
                                          args[i] = gcc_jit_context_new_cast(ctx, NULL, vptr, param_type);
                                      }
                                  }
                              }
                              *out_rval = gcc_jit_context_new_call(ctx, NULL, func, num_args, args);
                          } else if (fn_ptr) {
                              *out_rval = gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, num_args, args);
                          } else {
                              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
                          }
                          
                          if (args) free(args);
                          return GCC_GEN_BE_SUCCESS;
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
                          
                          gcc_jit_function *bfunc = gcc_jit_context_get_builtin_function(ctx, bname);
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
                                  if (!args) return GCC_GEN_BE_ERROR_OOM;
                                  int i = 0;
                                  arg = op1;
                                  while (arg) {
                                      err = gcc_gen_be_lower_expr_rvalue(arg, &args[i]);
                                      if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                      i++;
                                      arg = arg->next;
                                  }
                              }
                              
                              *out_rval = gcc_jit_context_new_call(ctx, NULL, bfunc, num_args, args);
                              if (args) free(args);
                              return GCC_GEN_BE_SUCCESS;
                          }
                          *out_rval = gcc_jit_context_zero(ctx, type);
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_bool_cast:
                      {
                          gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                          *out_rval = gcc_jit_context_new_cast(ctx, NULL, r1, bool_type);
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_array_to_pointer:
                      {
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          if (l1) {
                              *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(l1, NULL), type);
                          } else {
                              *out_rval = gcc_jit_context_null(ctx, type);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_lvalue_cast:
                  case eok_ref_cast:
                  case eok_lvalue_adjust:
                  case eok_class_rvalue_adjust:
                  case eok_cast:
                      {
                          if (p_gcc_jit_context_new_bitcast && expr->type && op1 && op1->type &&
                              ((expr->type->kind == tk_float && op1->type->kind == tk_integer) ||
                               (expr->type->kind == tk_integer && op1->type->kind == tk_float))) {
                              /* Bitcast between float and int of same size */
                              *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, r1, type);
                          } else {
                              *out_rval = gcc_jit_context_new_cast(ctx, NULL, r1, type);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  default:
                      return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
              }
          }
          
      case enk_typeid:
          {
              err = gcc_gen_be_lower_typeid(expr, out_rval);
              if (err != GCC_GEN_BE_SUCCESS) return err;
          }
          break;

      case enk_routine:
          {
              a_routine_ptr rout = expr->variant.routine.ptr;
              void *cached = NULL;
              err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_function *f = (gcc_jit_function *)cached;
              if (!f) {
                  err = gcc_gen_be_lower_function_decl(rout, &f);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
              }
              if (f) *out_rval = gcc_jit_function_get_address(f, NULL);
              return GCC_GEN_BE_SUCCESS;
          }
          
      case enk_throw:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (cblock) {
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
                      /* Wait, we just copy the memory or evaluate the expression. */
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

                      gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, throw_fn, 3, args));
                  } else {
                      /* Rethrow */
                      gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                      gcc_jit_function *rethrow_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_rethrow", 0, NULL, 0);
                      gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, rethrow_fn, 0, NULL));
                  }
                  
                  /* Control flow terminated after throw */
                  gcc_jit_block *unreachable_block = gcc_jit_function_new_block(gcc_jit_block_get_function(cblock), "unreachable_after_throw");
                  gcc_jit_block_end_with_jump(cblock, NULL, unreachable_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(unreachable_block));
              }
              
              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
          }
          
      default:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
          }
  }
    return GCC_GEN_BE_SUCCESS;
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */