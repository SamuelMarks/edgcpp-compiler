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
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/** @brief Points to the local variable capturing the result of the current statement expression. */
struct gcc_jit_lvalue *current_stmt_expr_result_lval = NULL;





gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!expr) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  gcc_gen_be_error_t err;

  gcc_jit_location *loc = NULL;
  gcc_gen_be_get_location_from_expr(expr, &loc);

  switch (expr->kind) {

      case enk_temp_init:
      case enk_lambda:
          {
              gcc_jit_type *temp_type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &temp_type);
              if (err != GCC_GEN_BE_SUCCESS) return err;

              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_lvalue *temp_lval = gcc_jit_function_new_local(func, NULL, temp_type, "temp_init");

              a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
              if (dip && dip->kind != dik_none) {
                  gcc_jit_rvalue *init_rval = NULL;
                  if (dip->kind == dik_zero) {
                      err = gcc_gen_be_get_zero_rvalue(expr->type, temp_type, &init_rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  } else if (dip->kind == dik_constant || dip->kind == dik_nonconstant_aggregate) {
                      a_constant_ptr con = dip->variant.constant.ptr;
                      err = gcc_gen_be_lower_constant_rvalue(con, temp_type, &init_rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  } else if (dip->kind == dik_expression || dip->kind == dik_class_result_via_ctor) {
                      err = gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &init_rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  
                  if (init_rval) {
                      gcc_jit_block_add_assignment(cblock, loc, temp_lval, init_rval);
                  }
                  
                  if (dip->destructor) {
                      void *dtor_cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, dip->destructor, &dtor_cached);
                      if (err == GCC_GEN_BE_SUCCESS && dtor_cached) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(temp_lval, (gcc_jit_function *)dtor_cached));
                      }
                  }
              }

              *out_lval = temp_lval;
              return GCC_GEN_BE_SUCCESS;
          }

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
                  *out_lval = gcc_jit_rvalue_dereference(ptr, loc);
                  return GCC_GEN_BE_SUCCESS;
                } else if (op == eok_dot_field || op == eok_points_to_field) {
                    if (op2 && op2->kind == enk_field) {
                        a_field_ptr f = op2->variant.field.ptr;
                        if (f->is_bit_field || f->bit_size > 0) {
                            *out_lval = NULL;
                            return GCC_GEN_BE_SUCCESS;
                        }
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
                            *out_lval = gcc_jit_lvalue_access_field(base, loc, field);
                        } else {
                            gcc_jit_rvalue *ptr = NULL;
                            err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            *out_lval = gcc_jit_rvalue_dereference_field(ptr, loc, field);
                        }
                        return GCC_GEN_BE_SUCCESS;
                    } else {
                        /* Pointer-to-data-member with offset */
                        gcc_jit_rvalue *offset_rval = NULL;
                        err = gcc_gen_be_lower_expr_rvalue(op2, &offset_rval);
                        if (err != GCC_GEN_BE_SUCCESS) return err;

                        gcc_jit_rvalue *base_rval = NULL;
                        if (op == eok_dot_field) {
                            gcc_jit_lvalue *base_lval = NULL;
                            err = gcc_gen_be_lower_expr_lvalue(op1, &base_lval);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            base_rval = gcc_jit_lvalue_get_address(base_lval, NULL);
                        } else {
                            err = gcc_gen_be_lower_expr_rvalue(op1, &base_rval);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                        }

                        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                        gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                        
                        gcc_jit_rvalue *base_char_ptr = gcc_jit_context_new_cast(ctx, loc, base_rval, char_ptr_type);
                        gcc_jit_lvalue *arr_access = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, offset_rval);
                        gcc_jit_rvalue *byte_ptr = gcc_jit_lvalue_get_address(arr_access, NULL);
                        
                        gcc_jit_type *dest_type = NULL;
                        err = gcc_gen_be_lower_type(expr->type, &dest_type);
                        if (err != GCC_GEN_BE_SUCCESS) return err;
                        
                        gcc_jit_type *dest_ptr_type = gcc_jit_type_get_pointer(dest_type);
                        gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, dest_ptr_type);
                        
                        *out_lval = gcc_jit_rvalue_dereference(typed_ptr, loc);
                        return GCC_GEN_BE_SUCCESS;
                    }
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
                          ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(arr_lval, NULL), elem_ptr_type);
                      }
                  }
                  if (!ptr) {
                      err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  
                  gcc_jit_rvalue *idx = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &idx);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  *out_lval = gcc_jit_context_new_array_access(ctx, loc, ptr, idx);
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

/**
 * @brief Lowers a bitfield read operation.
 * @details Extracts and aligns a bitfield value.
 * @param[in] expr The expression to lower.
 * @param[out] out_rval The lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_bitfield_read(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1->next;
    a_field_ptr f = op2->variant.field.ptr;
    
    gcc_jit_rvalue *base_rval = NULL;
    if (op == eok_dot_field) {
        gcc_jit_lvalue *base_lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &base_lval));
        base_rval = gcc_jit_lvalue_get_address(base_lval, NULL);
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &base_rval));
    }
    
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
    gcc_jit_rvalue *base_char_ptr = gcc_jit_context_new_cast(ctx, loc, base_rval, char_ptr_type);
    
    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, f->offset);
    gcc_jit_lvalue *byte_lval = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, offset);
    gcc_jit_rvalue *byte_ptr = gcc_jit_lvalue_get_address(byte_lval, NULL);
    
    gcc_jit_type *word_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &word_type));
    gcc_jit_type *word_ptr_type = gcc_jit_type_get_pointer(word_type);
    gcc_jit_rvalue *word_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, word_ptr_type);
    gcc_jit_rvalue *word_val = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(word_ptr, NULL));
    
    if (f->offset_bit_remainder > 0) {
        gcc_jit_rvalue *shift = gcc_jit_context_new_rvalue_from_long(ctx, word_type, f->offset_bit_remainder);
        word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, word_val, shift);
    }
    
    unsigned long mask_val = (1UL << f->bit_size) - 1;
    gcc_jit_rvalue *mask = gcc_jit_context_new_rvalue_from_long(ctx, word_type, mask_val);
    word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, word_type, word_val, mask);
    
    if (f->bit_field_is_signed) {
        int word_bits = (int)gcc_jit_type_get_size(word_type) * 8;
        if (word_bits == 0) word_bits = 32; /* Fallback */
        int shift_amt = word_bits - f->bit_size;
        gcc_jit_rvalue *s_amt = gcc_jit_context_new_rvalue_from_long(ctx, word_type, shift_amt);
        word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, word_type, word_val, s_amt);
        word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, word_val, s_amt);
    }
    
    *out_rval = word_val;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a bitfield assignment operation.
 * @details Reads, masks, shifts, and stores a new bitfield value.
 * @param[in] bf_expr The bitfield expression.
 * @param[in] assign_op The assignment operator.
 * @param[in] rhs_expr The RHS expression.
 * @param[out] out_rval The lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_bitfield_assignment(an_expr_node_ptr bf_expr, an_expr_operator_kind assign_op, an_expr_node_ptr rhs_expr, gcc_jit_rvalue **out_rval) {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    an_expr_operator_kind op = bf_expr->variant.operation.kind;
    an_expr_node_ptr op1 = bf_expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1->next;
    a_field_ptr f = op2->variant.field.ptr;
    
    gcc_jit_rvalue *base_rval = NULL;
    if (op == eok_dot_field) {
        gcc_jit_lvalue *base_lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &base_lval));
        base_rval = gcc_jit_lvalue_get_address(base_lval, NULL);
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &base_rval));
    }
    
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
    gcc_jit_rvalue *base_char_ptr = gcc_jit_context_new_cast(ctx, loc, base_rval, char_ptr_type);
    
    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, f->offset);
    gcc_jit_lvalue *byte_lval = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, offset);
    gcc_jit_rvalue *byte_ptr = gcc_jit_lvalue_get_address(byte_lval, NULL);
    
    gcc_jit_type *word_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &word_type));
    gcc_jit_type *word_ptr_type = gcc_jit_type_get_pointer(word_type);
    gcc_jit_rvalue *word_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, word_ptr_type);
    gcc_jit_lvalue *word_lval = gcc_jit_rvalue_dereference(word_ptr, NULL);
    
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    
    gcc_jit_rvalue *rhs_val = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(rhs_expr, &rhs_val));
    rhs_val = gcc_jit_context_new_cast(ctx, loc, rhs_val, word_type);
    
    if (assign_op != eok_assign) {
        gcc_jit_rvalue *current_bf_val = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_bitfield_read(bf_expr, &current_bf_val));
        
        enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_PLUS;
        switch (assign_op) {
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
            default: break;
        }
        rhs_val = gcc_jit_context_new_binary_op(ctx, loc, jit_op, word_type, current_bf_val, rhs_val);
    }
    
    unsigned long mask_val = (1UL << f->bit_size) - 1;
    gcc_jit_rvalue *mask = gcc_jit_context_new_rvalue_from_long(ctx, word_type, mask_val);
    rhs_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, word_type, rhs_val, mask);
    
    if (f->offset_bit_remainder > 0) {
        gcc_jit_rvalue *shift = gcc_jit_context_new_rvalue_from_long(ctx, word_type, f->offset_bit_remainder);
        rhs_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, word_type, rhs_val, shift);
    }
    
    unsigned long clear_mask_val = ~(mask_val << f->offset_bit_remainder);
    gcc_jit_rvalue *clear_mask = gcc_jit_context_new_rvalue_from_long(ctx, word_type, clear_mask_val);
    
    gcc_jit_rvalue *old_word = gcc_jit_lvalue_as_rvalue(word_lval);
    gcc_jit_rvalue *cleared_word = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, word_type, old_word, clear_mask);
    
    gcc_jit_rvalue *new_word = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_OR, word_type, cleared_word, rhs_val);
    gcc_jit_block_add_assignment(cblock, loc, word_lval, new_word);
    
    *out_rval = rhs_val; // This might need to be shifted back if we return it. Wait, the result of a bitfield assignment is the assigned value.
    if (f->offset_bit_remainder > 0) {
        gcc_jit_rvalue *shift = gcc_jit_context_new_rvalue_from_long(ctx, word_type, f->offset_bit_remainder);
        *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, rhs_val, shift);
    }
    if (f->bit_field_is_signed) {
        int word_bits = (int)gcc_jit_type_get_size(word_type) * 8;
        if (word_bits == 0) word_bits = 32;
        int shift_amt = word_bits - f->bit_size;
        gcc_jit_rvalue *s_amt = gcc_jit_context_new_rvalue_from_long(ctx, word_type, shift_amt);
        *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, word_type, *out_rval, s_amt);
        *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, *out_rval, s_amt);
    }
    
    return GCC_GEN_BE_SUCCESS;
}
/**
 * @brief Helper to cast a value between pointer and integer.
 * @details Performs a safe pointer-to-int or int-to-pointer cast.
 * @param[in] ctx The libgccjit context.
 * @param[in] val The value to cast.
 * @param[in] dest_type The destination type.
 * @param[out] out_rval The casted value.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t safe_ptr_or_int_cast(gcc_jit_context *ctx, gcc_jit_rvalue *val, gcc_jit_type *dest_type, gcc_jit_rvalue **out_rval) {
    if (!val || !dest_type || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(val);
    if (src_type == dest_type) {
        *out_rval = val;
        return GCC_GEN_BE_SUCCESS;
    }
#if defined(LIBGCCJIT_HAVE_gcc_jit_context_new_bitcast)
    size_t s1 = gcc_jit_type_get_size(src_type);
    size_t s2 = gcc_jit_type_get_size(dest_type);
    if (s1 == s2 && s1 > 0) {
        *out_rval = gcc_jit_context_new_bitcast(ctx, NULL, val, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
#endif
    *out_rval = gcc_jit_context_new_cast(ctx, loc, val, dest_type);
    return GCC_GEN_BE_SUCCESS;
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
    } else if (con->variant.address.kind == abk_label) {
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        gcc_jit_rvalue *val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(uintptr_t)con->variant.address.variant.label);
        *out_rval = gcc_jit_context_new_cast(ctx, loc, val, expected_type);
        return GCC_GEN_BE_SUCCESS;
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

        gcc_jit_rvalue *c1;
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, base_rval, long_type, &c1));
        gcc_jit_rvalue *c2 = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)con->variant.address.offset);
        gcc_jit_rvalue *add = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, long_type, c1, c2);

        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, add, expected_type, out_rval));
    } else {
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, base_rval, expected_type, out_rval));
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
            gcc_gen_be_error_t err = safe_ptr_or_int_cast(ctx, gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), 0), expected_type, out_rval);
            if (err != GCC_GEN_BE_SUCCESS || !*out_rval) {
                (void)gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
            }
        } else {
            gcc_jit_rvalue *num = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), val);
            GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, num, expected_type, out_rval));
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

  gcc_jit_location *loc = NULL;
  gcc_gen_be_get_location_from_expr(expr, &loc);

  switch (expr->kind) {

      case enk_temp_init:
      case enk_lambda:
          {
              gcc_jit_lvalue *temp_lval = NULL;
              err = gcc_gen_be_lower_expr_lvalue(expr, &temp_lval);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (temp_lval) {
                  *out_rval = gcc_jit_lvalue_as_rvalue(temp_lval);
              } else {
                  *out_rval = NULL;
              }
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_builtin_operation:
          {
              a_builtin_operation_kind bok = expr->variant.builtin_operation.kind;
              if (bok == bok_builtin_complex) {
                  an_expr_node_ptr r_expr = expr->variant.builtin_operation.operands;
                  an_expr_node_ptr i_expr = r_expr->next;
                  
                  gcc_jit_rvalue *r_val = NULL;
                  gcc_jit_rvalue *i_val = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(r_expr, &r_val));
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(i_expr, &i_val));
                  
                  gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  if (gcc_jit_type_is_struct(c_type)) {
                      gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), r_val);
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), i_val);
                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                  } else {
                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                      gcc_jit_type *elem_type = gcc_jit_rvalue_get_type(r_val);
                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                      
                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                      gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                      gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                      
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero), r_val);
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one), i_val);
                      
                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_statement:
          {
              gcc_jit_type *res_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              
              gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, res_type, "stmt_expr_res");
              
              extern struct gcc_jit_lvalue *current_stmt_expr_result_lval;
              struct gcc_jit_lvalue *old_res_lval = current_stmt_expr_result_lval;
              current_stmt_expr_result_lval = res_lval;
              
              extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(expr->variant.statement, func));
              
              current_stmt_expr_result_lval = old_res_lval;
              
              *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
              return GCC_GEN_BE_SUCCESS;
          }
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
                      *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
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
                          *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
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
                  gcc_jit_block_end_with_conditional(cblock, loc, gcc_jit_context_new_cast(ctx, loc, r1, bool_type), rhs_block, false_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(false_block));
                  gcc_jit_block_add_assignment(false_block, NULL, res, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 0));
                  gcc_jit_block_end_with_jump(false_block, loc, merge_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rhs_block));
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&rhs_block));
                  if (rhs_block) {
                      gcc_jit_block_add_assignment(rhs_block, NULL, res, gcc_jit_context_new_cast(ctx, loc, r2, bool_type));
                      gcc_jit_block_end_with_jump(rhs_block, loc, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(res), type);
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
                  gcc_jit_block_end_with_conditional(cblock, loc, gcc_jit_context_new_cast(ctx, loc, r1, bool_type), true_block, rhs_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(true_block));
                  gcc_jit_block_add_assignment(true_block, NULL, res, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 1));
                  gcc_jit_block_end_with_jump(true_block, loc, merge_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rhs_block));
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&rhs_block));
                  if (rhs_block) {
                      gcc_jit_block_add_assignment(rhs_block, NULL, res, gcc_jit_context_new_cast(ctx, loc, r2, bool_type));
                      gcc_jit_block_end_with_jump(rhs_block, loc, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(res), type);
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
                  gcc_jit_block_end_with_conditional(cblock, loc, gcc_jit_context_new_cast(ctx, loc, r1, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)), then_block, else_block);
                  
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
                          gcc_jit_block_add_assignment(cblock, loc, l1, r2);
                      } else if (op == eok_padd_assign) {
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(l1), r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
                      } else if (op == eok_psubtract_assign) {
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, gcc_jit_rvalue_get_type(r2), r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(l1), neg_r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
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
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, ptr_val, int_val);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_psubtract:
                      {
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, gcc_jit_rvalue_get_type(r2), r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, r1, neg_r2);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_pdiff:
                      {
                          gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *c1 = gcc_jit_context_new_cast(ctx, loc, r1, long_type);
                          gcc_jit_rvalue *c2 = gcc_jit_context_new_cast(ctx, loc, r2, long_type);
                          gcc_jit_rvalue *diff = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, long_type, c1, c2);
                          
                          a_type_ptr elem_type = op1->type->variant.pointer.type;
                          long sz = (long)size_of_type(elem_type);
                          gcc_jit_rvalue *sz_val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, sz);
                          
                          *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, long_type, diff, sz_val);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_spaceship:
                      {
                          gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                          gcc_jit_rvalue *cmp_lt = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, r1, r2);
                          gcc_jit_rvalue *cmp_gt = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GT, r1, r2);
                          gcc_jit_rvalue *lt_val = gcc_jit_context_new_cast(ctx, loc, cmp_lt, int_type);
                          gcc_jit_rvalue *gt_val = gcc_jit_context_new_cast(ctx, loc, cmp_gt, int_type);
                          gcc_jit_rvalue *res_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, int_type, gt_val, lt_val);

                          a_type_ptr op_t = op1->type;
                          while (op_t && op_t->kind == tk_typeref) op_t = op_t->variant.typeref.type;
                          if (op_t && (op_t->kind == tk_float)) {
                              gcc_jit_rvalue *cmp_ne = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, r2);
                              gcc_jit_rvalue *cmp_unordered = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, int_type,
                                  gcc_jit_context_new_cast(ctx, loc, cmp_ne, int_type),
                                  gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, int_type, res_val));

                              /* If unordered (not equal and not < and not >), return -127 */
                              gcc_jit_rvalue *unord_val = gcc_jit_context_new_rvalue_from_int(ctx, int_type, -127);

                              /* We want a ternary operator or just if unordered ? -127 : res_val */
                              /* In libgccjit, there's no ternary rvalue yet in some older versions, so let's do bitwise or arithmetic:
                                 res = unord ? -127 : res_val;
                                 res = (unord * -127) + ((1 - unord) * res_val)
                                 Wait, unord is 1 or 0 */
                              gcc_jit_rvalue *unord_term = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, int_type, cmp_unordered, unord_val);
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                              gcc_jit_rvalue *not_unord = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, int_type, one, cmp_unordered);
                              gcc_jit_rvalue *res_term = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, int_type, not_unord, res_val);
                              res_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, int_type, unord_term, res_term);
                          }

                          /* `type` is expected to be the comparison category struct. We can cast the int to signed char, then bitcast to struct. */
                          gcc_jit_type *schar_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR);
                          gcc_jit_rvalue *schar_val = gcc_jit_context_new_cast(ctx, loc, res_val, schar_type);

                          if (gcc_jit_type_is_struct(type)) {
                  #if defined(LIBGCCJIT_HAVE_gcc_jit_context_new_bitcast)
                              *out_rval = gcc_jit_context_new_bitcast(ctx, NULL, schar_val, type);
                  #else
                              /* Create a temporary variable of the struct type and write the byte */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              if (cblock) {
                                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_lvalue *tmp_lval = gcc_jit_function_new_local(func, NULL, type, "spaceship_tmp");
                                  gcc_jit_type *schar_ptr_type = gcc_jit_type_get_pointer(schar_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp_lval, NULL);
                                  gcc_jit_rvalue *tmp_schar_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, schar_ptr_type);
                                  gcc_jit_lvalue *tmp_schar_lval = gcc_jit_rvalue_dereference(tmp_schar_ptr, NULL);
                                  gcc_jit_block_add_assignment(cblock, loc, tmp_schar_lval, schar_val);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp_lval);
                              } else {
                                  *out_rval = schar_val; /* fallback */
                              }
                  #endif
                          } else {
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, schar_val, type);
                          }

                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_add:
                  case eok_subtract:
                  case eok_multiply:
                  case eok_divide:
                      {
                          extern a_boolean is_complex_type(a_type_ptr tp);
                          if (is_complex_type(expr->type)) {
                              gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                              gcc_jit_type *elem_type = NULL;
                              a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              /* Helper to get real/imag */
                              auto get_parts = [&](gcc_jit_rvalue *val, gcc_jit_rvalue **r, gcc_jit_rvalue **i) {
                                  if (gcc_jit_type_is_struct(c_type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                      *r = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 0));
                                      *i = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 1));
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                                      gcc_jit_block_add_assignment(cblock, loc, tmp, val);
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                                      gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                                      *r = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero));
                                      *i = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one));
                                  }
                              };
                              
                              gcc_jit_rvalue *a, *b, *c, *d;
                              get_parts(r1, &a, &b);
                              get_parts(r2, &c, &d);
                              
                              gcc_jit_rvalue *res_r = NULL;
                              gcc_jit_rvalue *res_i = NULL;
                              
                              if (op == eok_add) {
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, a, c);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, b, d);
                              } else if (op == eok_subtract) {
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, a, c);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, b, d);
                              } else if (op == eok_multiply) {
                                  /* (a+bi)(c+di) = (ac-bd) + (ad+bc)i */
                                  gcc_jit_rvalue *ac = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, c);
                                  gcc_jit_rvalue *bd = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, d);
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, ac, bd);
                                  
                                  gcc_jit_rvalue *ad = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, d);
                                  gcc_jit_rvalue *bc = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, c);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, ad, bc);
                              } else if (op == eok_divide) {
                                  /* standard algorithm: (a+bi)/(c+di) = (ac+bd)/(c^2+d^2) + (bc-ad)/(c^2+d^2) i */
                                  gcc_jit_rvalue *ac = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, c);
                                  gcc_jit_rvalue *bd = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, d);
                                  gcc_jit_rvalue *num_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, ac, bd);
                                  
                                  gcc_jit_rvalue *bc = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, c);
                                  gcc_jit_rvalue *ad = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, d);
                                  gcc_jit_rvalue *num_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, bc, ad);
                                  
                                  gcc_jit_rvalue *c2 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, c, c);
                                  gcc_jit_rvalue *d2 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, d, d);
                                  gcc_jit_rvalue *den = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, c2, d2);
                                  
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, elem_type, num_r, den);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, elem_type, num_i, den);
                              }
                              
                              if (gcc_jit_type_is_struct(c_type)) {
                                  gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), res_r);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), res_i);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              } else {
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                  gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                  
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                                  gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                                  
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero), res_r);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one), res_i);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              }
                              return GCC_GEN_BE_SUCCESS;
                          } else {
                              enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_PLUS;
                              if (op == eok_subtract) jit_op = GCC_JIT_BINARY_OP_MINUS;
                              else if (op == eok_multiply) jit_op = GCC_JIT_BINARY_OP_MULT;
                              else if (op == eok_divide) jit_op = GCC_JIT_BINARY_OP_DIVIDE;
                              *out_rval = gcc_jit_context_new_binary_op(ctx, loc, jit_op, type, r1, r2);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_remainder: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MODULO, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_shiftl: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_shiftr: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_comma:
                      {
                          gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (r1 && cblock) gcc_jit_block_add_eval(cblock, loc, r1);
                          *out_rval = r2;
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_and: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_or: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_OR, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_xor: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_XOR, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                      
                  case eok_eq:
                  case eok_ne:
                      {
                          extern a_boolean is_complex_type(a_type_ptr tp);
                          if (op1 && is_complex_type(op1->type)) {
                              gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                              gcc_jit_type *elem_type = NULL;
                              a_type_ptr op1_t2 = op1->type;
while (op1_t2 && op1_t2->kind == tk_typeref) op1_t2 = op1_t2->variant.typeref.type;
if (op1_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op1_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              auto get_parts = [&](gcc_jit_rvalue *val, gcc_jit_rvalue **r, gcc_jit_rvalue **i) {
                                  if (gcc_jit_type_is_struct(c_type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                      *r = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 0));
                                      *i = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 1));
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                                      gcc_jit_block_add_assignment(cblock, loc, tmp, val);
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      *r = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)));
                                      *i = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)));
                                  }
                              };
                              
                              gcc_jit_rvalue *a, *b, *c, *d;
                              get_parts(r1, &a, &b);
                              get_parts(r2, &c, &d);
                              
                              gcc_jit_rvalue *r_eq = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, a, c);
                              gcc_jit_rvalue *i_eq = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, b, d);
                              
                              gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                              gcc_jit_rvalue *r_eq_b = gcc_jit_context_new_cast(ctx, loc, r_eq, bool_type);
                              gcc_jit_rvalue *i_eq_b = gcc_jit_context_new_cast(ctx, loc, i_eq, bool_type);
                              
                              gcc_jit_rvalue *both_eq = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_type, r_eq_b, i_eq_b);
                              
                              if (op == eok_eq) {
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, both_eq, type);
                              } else {
                                  gcc_jit_rvalue *neq_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_type, both_eq);
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, neq_val, type);
                              }
                              return GCC_GEN_BE_SUCCESS;
                          } else {
                              *out_rval = gcc_jit_context_new_comparison(ctx, loc, op == eok_eq ? GCC_JIT_COMPARISON_EQ : GCC_JIT_COMPARISON_NE, r1, r2);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_lt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_le: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_gt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_ge: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  
                  case eok_fjadd:
                  case eok_jfadd:
                  case eok_fjsubtract:
                  case eok_jfsubtract:
                      {
                          gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                          gcc_jit_type *elem_type = NULL;
                          a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                          else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                          else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                          
                          gcc_jit_rvalue *real_val = (op == eok_fjadd || op == eok_fjsubtract) ? r1 : r2;
                          gcc_jit_rvalue *imag_val = (op == eok_fjadd || op == eok_fjsubtract) ? r2 : r1;
                          
                          if (op == eok_fjsubtract) {
                              imag_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_val);
                          } else if (op == eok_jfsubtract) {
                              real_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, real_val);
                          }
                          
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                          
                          if (gcc_jit_type_is_struct(c_type)) {
                              gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), real_val);
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), imag_val);
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          } else {
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                              gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                              gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                              gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                              
                              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                              gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                              
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero), real_val);
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one), imag_val);
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_vector_eq: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_ne: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_lt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_le: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_gt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_ge: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_land: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_lor: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_OR, type, r1, r2); return GCC_GEN_BE_SUCCESS;

                  case eok_vector_not: *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_negate:
                      {
                          extern a_boolean is_complex_type(a_type_ptr tp);
                          if (is_complex_type(expr->type)) {
                              gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                              gcc_jit_type *elem_type = NULL;
                              a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              gcc_jit_rvalue *a, *b;
                              if (gcc_jit_type_is_struct(c_type)) {
                                  gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                  a = gcc_jit_rvalue_access_field(r1, NULL, gcc_jit_struct_get_field(s, 0));
                                  b = gcc_jit_rvalue_access_field(r1, NULL, gcc_jit_struct_get_field(s, 1));
                              } else {
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                                  gcc_jit_block_add_assignment(cblock, loc, tmp, r1);
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                  gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  a = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)));
                                  b = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)));
                              }
                              
                              gcc_jit_rvalue *neg_a = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, a);
                              gcc_jit_rvalue *neg_b = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, b);
                              
                              if (gcc_jit_type_is_struct(c_type)) {
                                  gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), neg_a);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), neg_b);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              } else {
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                  gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)), neg_a);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)), neg_b);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              }
                              return GCC_GEN_BE_SUCCESS;
                          } else {
                              *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, type, r1);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_complement: *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_BITWISE_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_not: *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;

                  case eok_real_part:
                  case eok_imag_part:
                      {
                          gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                          if (gcc_jit_type_is_struct(c_type)) {
                              gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                              gcc_jit_field *f = gcc_jit_struct_get_field(s, op == eok_real_part ? 0 : 1);
                              *out_rval = gcc_jit_rvalue_access_field(r1, NULL, f);
                          } else {
                              /* Fallback for native complex types (bitcast or array access workaround) */
                              gcc_jit_type *elem_type = type; /* The result type is the element type */
                              gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                              
                              /* We need to get the address of r1. If r1 is an lvalue-as-rvalue or similar, we might need a local */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, tmp, r1);
                              
                              gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                              gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                              
                              if (op == eok_imag_part) {
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                                  gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(arr_acc);
                              } else {
                                  *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(elem_ptr, NULL));
                              }
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_xconj:
                      {
                          gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                          gcc_jit_type *elem_type = NULL;
                          if (gcc_jit_type_is_struct(c_type)) {
                              gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                              gcc_jit_field *f_real = gcc_jit_struct_get_field(s, 0);
                              gcc_jit_field *f_imag = gcc_jit_struct_get_field(s, 1);
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(op2->variant.field.ptr->type, &elem_type));
                              
                              gcc_jit_rvalue *real_val = gcc_jit_rvalue_access_field(r1, NULL, f_real);
                              gcc_jit_rvalue *imag_val = gcc_jit_rvalue_access_field(r1, NULL, f_imag);
                              gcc_jit_rvalue *neg_imag = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_val);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "conj_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, f_real), real_val);
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, f_imag), neg_imag);
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          } else {
                              /* If native complex type, libgccjit supports bitwise conjugate via MINUS? No, there is no conjugate unary op. */
                              /* We can negate the imaginary part via pointer as before */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "conj_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, tmp, r1);
                              
                              gcc_jit_type *elem_ptr_type = NULL;
                              a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                              gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                              gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                              
                              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                              gcc_jit_lvalue *imag_lval = gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one);
                              gcc_jit_rvalue *imag_rval = gcc_jit_lvalue_as_rvalue(imag_lval);
                              gcc_jit_rvalue *neg_imag = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_rval);
                              gcc_jit_block_add_assignment(cblock, loc, imag_lval, neg_imag);
                              
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }


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
                          
                          gcc_jit_block_add_assignment(cblock, loc, temp, gcc_jit_lvalue_as_rvalue(l1));
                          
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
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lexpr, NULL), elem_ptr_type);
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
                          a_boolean uses_sret = FALSE;
                          gcc_jit_lvalue *sret_lval = NULL;
                          if (op1->kind == enk_routine) {
                              extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
                              err = gcc_gen_be_is_returned_via_sret(op1->variant.routine.ptr->type->variant.routine.return_type, &uses_sret);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              if (uses_sret) num_args++;
                          }

                          if (num_args > 0) {
                              args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                              if (!args) return GCC_GEN_BE_ERROR_OOM;
                              
                              int i = 0;
                              if (uses_sret) {
                                  gcc_jit_block *cblock = NULL;
                                  gcc_gen_be_get_current_block(&cblock);
                                  gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_type *sret_type = NULL;
                                  gcc_gen_be_lower_type(op1->variant.routine.ptr->type->variant.routine.return_type, &sret_type);
                                  sret_lval = gcc_jit_function_new_local(parent_func, NULL, sret_type, "sret_buffer");
                                  args[i++] = gcc_jit_lvalue_get_address(sret_lval, NULL);
                              }
                              
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
                                  gcc_jit_rvalue *arg_rval = NULL;
                                  err = gcc_gen_be_lower_expr_rvalue(arg, &arg_rval);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  
                                  extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
                                  a_boolean is_invis_ref = FALSE;
                                  err = gcc_gen_be_is_passed_by_invisible_reference(arg->type, &is_invis_ref);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  if (is_invis_ref) {
                                      gcc_jit_block *cblock = NULL;
                                      gcc_gen_be_get_current_block(&cblock);
                                      gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                      gcc_jit_type *arg_type = gcc_jit_rvalue_get_type(arg_rval);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(parent_func, NULL, arg_type, "invis_ref");
                                      gcc_jit_block_add_assignment(cblock, loc, tmp, arg_rval);
                                      arg_rval = gcc_jit_lvalue_get_address(tmp, NULL);
                                  }
                                  
                                  args[i] = arg_rval;
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

                            if (is_member && op1->kind != enk_routine && num_args > 0) {
                                gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                                gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                                gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                                gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                
                                gcc_jit_type *struct_type = gcc_jit_rvalue_get_type(fn_ptr);
                                gcc_jit_field *f_func_ptr = gcc_jit_struct_get_field(gcc_jit_type_is_struct(struct_type), 0);
                                gcc_jit_field *f_this_adj = gcc_jit_struct_get_field(gcc_jit_type_is_struct(struct_type), 1);
                                
                                gcc_jit_rvalue *func_ptr_val = gcc_jit_rvalue_access_field(fn_ptr, NULL, f_func_ptr);
                                gcc_jit_rvalue *this_adj_val = gcc_jit_rvalue_access_field(fn_ptr, NULL, f_this_adj);
                                
                                /* Adjust this: (char*)this + this_adj */
                                gcc_jit_rvalue *this_char_ptr = gcc_jit_context_new_cast(ctx, loc, args[0], char_ptr_type);
                                gcc_jit_lvalue *adjusted_this = gcc_jit_context_new_array_access(ctx, loc, this_char_ptr, this_adj_val);
                                gcc_jit_rvalue *new_this_ptr = gcc_jit_lvalue_get_address(adjusted_this, NULL);
                                args[0] = gcc_jit_context_new_cast(ctx, loc, new_this_ptr, gcc_jit_rvalue_get_type(args[0]));
                                
                                /* Test least significant bit of func_ptr: (long)func_ptr & 1 */
                                gcc_jit_rvalue *func_ptr_as_long = gcc_jit_context_new_cast(ctx, loc, func_ptr_val, long_type);
                                gcc_jit_rvalue *one_long = gcc_jit_context_new_rvalue_from_long(ctx, long_type, 1);
                                gcc_jit_rvalue *is_virtual = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_type, func_ptr_as_long, one_long);
                                gcc_jit_rvalue *is_virtual_bool = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, is_virtual, gcc_jit_context_new_rvalue_from_long(ctx, long_type, 0));
                                
                                gcc_jit_block *cblock = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                
                                gcc_jit_block *virt_block = gcc_jit_function_new_block(parent_func, "pm_virt");
                                gcc_jit_block *non_virt_block = gcc_jit_function_new_block(parent_func, "pm_non_virt");
                                gcc_jit_block *merge_block = gcc_jit_function_new_block(parent_func, "pm_merge");
                                
                                gcc_jit_block_end_with_conditional(cblock, loc, is_virtual_bool, virt_block, non_virt_block);
                                
                                a_type_ptr op1_type = op1->type;
                                while (op1_type && op1_type->kind == tk_typeref) op1_type = op1_type->variant.typeref.type;
                                
                                gcc_jit_type *member_fn_type = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(op1_type->variant.ptr_to_member.type, &member_fn_type));
                                gcc_jit_type *fn_ptr_type = gcc_jit_type_get_pointer(member_fn_type);
                                
                                gcc_jit_lvalue *resolved_fn_ptr = gcc_jit_function_new_local(parent_func, NULL, fn_ptr_type, "resolved_fn_ptr");
                                
                                /* non-virtual: just cast func_ptr_val */
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(non_virt_block));
                                gcc_jit_block_add_assignment(non_virt_block, NULL, resolved_fn_ptr, gcc_jit_context_new_cast(ctx, loc, func_ptr_val, fn_ptr_type));
                                gcc_jit_block_end_with_jump(non_virt_block, NULL, merge_block);
                                
                                /* virtual: read vtable */
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(virt_block));
                                gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
                                gcc_jit_type *vtable_ptr_ptr_type = gcc_jit_type_get_pointer(vtable_ptr_type);
                                gcc_jit_rvalue *this_vptr_ptr = gcc_jit_context_new_cast(ctx, loc, new_this_ptr, vtable_ptr_ptr_type);
                                gcc_jit_rvalue *vptr = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(this_vptr_ptr, NULL));
                                
                                gcc_jit_rvalue *vtable_char_ptr = gcc_jit_context_new_cast(ctx, loc, vptr, char_ptr_type);
                                gcc_jit_rvalue *vfunc_offset = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, long_type, func_ptr_as_long, one_long);
                                gcc_jit_lvalue *vfunc_addr_lval = gcc_jit_context_new_array_access(ctx, loc, vtable_char_ptr, vfunc_offset);
                                gcc_jit_rvalue *vfunc_addr = gcc_jit_lvalue_get_address(vfunc_addr_lval, NULL);
                                
                                gcc_jit_rvalue *vfunc_void_ptr_ptr = gcc_jit_context_new_cast(ctx, loc, vfunc_addr, vtable_ptr_ptr_type);
                                gcc_jit_rvalue *vfunc_void_ptr = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(vfunc_void_ptr_ptr, NULL));
                                
                                gcc_jit_block_add_assignment(virt_block, NULL, resolved_fn_ptr, gcc_jit_context_new_cast(ctx, loc, vfunc_void_ptr, fn_ptr_type));
                                gcc_jit_block_end_with_jump(virt_block, NULL, merge_block);
                                
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                                fn_ptr = gcc_jit_lvalue_as_rvalue(resolved_fn_ptr);
                                func = NULL;
                            }

                          if (expr->variant.operation.is_virtual_call && op1->kind == enk_routine && num_args > 0) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              int vindex = rout->number.virtual_function;
                              if (vindex != VIRTUAL_FUNCTION_NUMBER_NONE) {
                                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                  gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
                                  gcc_jit_type *vtable_ptr_ptr_type = gcc_jit_type_get_pointer(vtable_ptr_type);
                                  
                                  gcc_jit_rvalue *this_ptr_casted = gcc_jit_context_new_cast(ctx, loc, args[0], vtable_ptr_ptr_type);
                                  gcc_jit_lvalue *vptr_lval = gcc_jit_rvalue_dereference(this_ptr_casted, NULL);
                                  gcc_jit_rvalue *vptr = gcc_jit_lvalue_as_rvalue(vptr_lval);
                                  
                                  gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 2 + vindex);
                                  gcc_jit_lvalue *fn_ptr_lval = gcc_jit_context_new_array_access(ctx, loc, vptr, idx_rval);
                                  gcc_jit_rvalue *raw_fn_ptr = gcc_jit_lvalue_as_rvalue(fn_ptr_lval);
                                  
                                  gcc_jit_type *fn_ptr_type = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type, &fn_ptr_type));
                                  fn_ptr = gcc_jit_context_new_cast(ctx, loc, raw_fn_ptr, fn_ptr_type);
                                  func = NULL;
                              }
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
                                          gcc_jit_rvalue *vptr = gcc_jit_context_new_cast(ctx, loc, args[i], void_ptr_type);
                                          args[i] = gcc_jit_context_new_cast(ctx, loc, vptr, param_type);
                                      }
                                  }
                              }
                                                            gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, loc, func, num_args, args);
                              if (uses_sret) {
                                  gcc_jit_block *cblock = NULL;
                                  gcc_gen_be_get_current_block(&cblock);
                                  gcc_jit_block_add_eval(cblock, loc, call_rval);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(sret_lval);
                              } else {
                                  *out_rval = call_rval;
                              }
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
                          if (op == eok_va_arg) {
                              /* Manual lowering for pointer-bumping va_list (AAPCS64 Apple, MS x64, x86 32-bit) */
                              gcc_jit_lvalue *ap_lval = NULL;
                              err = gcc_gen_be_lower_expr_lvalue(op1, &ap_lval);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              
                              gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                              gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                              
                              /* Read current ap: ap_val = (char*)*ap_lval */
                              gcc_jit_rvalue *ap_val = gcc_jit_lvalue_as_rvalue(ap_lval);
                              gcc_jit_rvalue *ap_char_ptr = gcc_jit_context_new_cast(ctx, loc, ap_val, char_ptr_type);
                              
                              /* size of type aligned to pointer size */
                              size_t type_size = gcc_jit_type_get_size(type);
                              size_t ptr_size = gcc_jit_type_get_size(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
                              size_t align = ptr_size;
                              size_t aligned_size = (type_size + align - 1) & ~(align - 1);
                              
                              gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                              gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, aligned_size);
                              
                              /* Compute next ap: next_ap = ap_char_ptr + offset */
                              gcc_jit_lvalue *next_ap_lval = gcc_jit_context_new_array_access(ctx, loc, ap_char_ptr, offset);
                              gcc_jit_rvalue *next_ap = gcc_jit_lvalue_get_address(next_ap_lval, NULL);
                              
                              /* Save next_ap back to ap_lval */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_rvalue *next_ap_casted = gcc_jit_context_new_cast(ctx, loc, next_ap, gcc_jit_rvalue_get_type(ap_val));
                              gcc_jit_block_add_assignment(cblock, loc, ap_lval, next_ap_casted);
                              
                              /* Cast original ap_val to type* and dereference */
                              gcc_jit_type *type_ptr = gcc_jit_type_get_pointer(type);
                              gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, ap_val, type_ptr);
                              *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, loc));
                              
                              return GCC_GEN_BE_SUCCESS;
                          }
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
                              
                              *out_rval = gcc_jit_context_new_call(ctx, loc, bfunc, num_args, args);
                              if (args) free(args);
                              return GCC_GEN_BE_SUCCESS;
                          }
                          *out_rval = gcc_jit_context_zero(ctx, type);
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_await:
          case eok_yield:
              {
                  an_expr_node_ptr rrs = expr->variant.await_info.ready_resume_suspend;
                  an_expr_node_ptr await_ready = rrs;
                  an_expr_node_ptr await_resume = rrs ? rrs->next : NULL;
                  an_expr_node_ptr await_suspend = await_resume ? await_resume->next : NULL;

                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);

                  gcc_jit_block *suspend_block = gcc_jit_function_new_block(func, "await_suspend");
                  gcc_jit_block *resume_block = gcc_jit_function_new_block(func, "await_resume");

                  gcc_jit_rvalue *is_ready = NULL;
                  if (await_ready) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_ready, &is_ready));
                  } else {
                      is_ready = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), 1);
                  }

                  /* Branch on boolean result of await_ready() */
                  gcc_jit_block_end_with_conditional(cblock, loc, is_ready, resume_block, suspend_block);

                  /* Inside suspend_block: Evaluate await_suspend() */
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(suspend_block));
                  if (await_suspend) {
                      gcc_jit_rvalue *suspend_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_suspend, &suspend_rval));
                      if (suspend_rval) {
                          gcc_jit_type *susp_type = gcc_jit_rvalue_get_type(suspend_rval);
                          /* Support void, bool, and symmetric transfer */
                          if (susp_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                              gcc_jit_block_add_eval(suspend_block, loc, suspend_rval);
                              gcc_jit_block_end_with_void_return(suspend_block, loc);
                          } else if (susp_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)) {
                              gcc_jit_block *suspend_actual = gcc_jit_function_new_block(func, "suspend_actual");
                              gcc_jit_block_end_with_conditional(suspend_block, loc, suspend_rval, suspend_actual, resume_block);
                              gcc_jit_block_end_with_void_return(suspend_actual, loc);
                          } else {
                              /* Symmetric transfer returning std::coroutine_handle<> */
                              /* In a real implementation we would call the handle's resume function */
                              gcc_jit_block_add_eval(suspend_block, loc, suspend_rval);
                              gcc_jit_block_end_with_void_return(suspend_block, loc);
                          }
                      } else {
                          gcc_jit_block_end_with_jump(suspend_block, NULL, resume_block);
                      }
                  } else {
                      gcc_jit_block_end_with_jump(suspend_block, NULL, resume_block);
                  }
                  
                  /* Resume block: evaluate await_resume() */
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(resume_block));
                  gcc_jit_rvalue *resume_rval = NULL;
                  if (await_resume) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_resume, &resume_rval));
                  }

                  if (resume_rval) {
                      *out_rval = resume_rval;
                  } else if (type && type->kind != tk_void) {
                      *out_rval = gcc_jit_context_zero(ctx, type);
                  } else {
                      *out_rval = NULL;
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
          case eok_bool_cast:
                      {
                          gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                          *out_rval = gcc_jit_context_new_cast(ctx, loc, r1, bool_type);
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_array_to_pointer:
                      {
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          if (l1) {
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(l1, NULL), type);
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
                          extern a_boolean is_complex_type(a_type_ptr tp);
                          a_boolean dest_is_complex = is_complex_type(expr->type);
                          a_boolean src_is_complex = op1 ? is_complex_type(op1->type) : FALSE;
                          
                          if (dest_is_complex || src_is_complex) {
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              auto get_parts = [&](gcc_jit_rvalue *val, gcc_jit_type *c_type, gcc_jit_type *elem_type, gcc_jit_rvalue **r, gcc_jit_rvalue **i) {
                                  if (gcc_jit_type_is_struct(c_type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                      *r = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 0));
                                      *i = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 1));
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                                      gcc_jit_block_add_assignment(cblock, loc, tmp, val);
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      *r = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)));
                                      *i = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)));
                                  }
                              };
                              
                              if (dest_is_complex && !src_is_complex) {
                                  /* Real to Complex */
                                  gcc_jit_type *elem_type = NULL;
                                  a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_rvalue *real_part = gcc_jit_context_new_cast(ctx, loc, r1, elem_type);
                                  gcc_jit_rvalue *imag_part = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
                                  imag_part = gcc_jit_context_new_cast(ctx, loc, imag_part, elem_type);
                                  
                                  if (gcc_jit_type_is_struct(type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(type);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), real_part);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), imag_part);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)), real_part);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)), imag_part);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  }
                                  return GCC_GEN_BE_SUCCESS;
                              } else if (!dest_is_complex && src_is_complex) {
                                  /* Complex to Real */
                                  gcc_jit_type *src_elem_type = NULL;
                                  a_type_ptr op1_t2 = op1->type;
while (op1_t2 && op1_t2->kind == tk_typeref) op1_t2 = op1_t2->variant.typeref.type;
if (op1_t2->variant.float_kind == fk_float) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op1_t2->variant.float_kind == fk_long_double) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_rvalue *r, *i;
                                  get_parts(r1, gcc_jit_rvalue_get_type(r1), src_elem_type, &r, &i);
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, r, type);
                                  return GCC_GEN_BE_SUCCESS;
                              } else if (dest_is_complex && src_is_complex) {
                                  /* Complex to Complex */
                                  gcc_jit_type *src_elem_type = NULL;
                                  a_type_ptr op1_t2 = op1->type;
while (op1_t2 && op1_t2->kind == tk_typeref) op1_t2 = op1_t2->variant.typeref.type;
if (op1_t2->variant.float_kind == fk_float) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op1_t2->variant.float_kind == fk_long_double) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_type *dest_elem_type = NULL;
                                  a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) dest_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op_t2->variant.float_kind == fk_long_double) dest_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else dest_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_rvalue *r, *i;
                                  get_parts(r1, gcc_jit_rvalue_get_type(r1), src_elem_type, &r, &i);
                                  
                                  gcc_jit_rvalue *new_r = gcc_jit_context_new_cast(ctx, loc, r, dest_elem_type);
                                  gcc_jit_rvalue *new_i = gcc_jit_context_new_cast(ctx, loc, i, dest_elem_type);
                                  
                                  if (gcc_jit_type_is_struct(type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(type);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), new_r);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), new_i);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(dest_elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)), new_r);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)), new_i);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  }
                                  return GCC_GEN_BE_SUCCESS;
                              }
                          }
                          if (p_gcc_jit_context_new_bitcast && expr->type && op1 && op1->type &&
                              ((expr->type->kind == tk_float && op1->type->kind == tk_integer) ||
                               (expr->type->kind == tk_integer && op1->type->kind == tk_float))) {
                              /* Bitcast between float and int of same size */
                              *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, r1, type);
                          } else {
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, r1, type);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  default:
                      return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
              }
          }


      case enk_braced_init_list:
          {
              *out_rval = NULL;
              gcc_jit_context *ctx = NULL;
              gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              
              gcc_jit_type *expected_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &expected_type));
              
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_lvalue *temp_lval = gcc_jit_function_new_local(func, NULL, expected_type, "braced_init_temp");
              
              an_expr_node_ptr elem = expr->variant.braced_init_list;
              
              a_type_ptr orig_type = expr->type;
              while (orig_type && orig_type->kind == tk_typeref) orig_type = orig_type->variant.typeref.type;
              
              if (orig_type->kind == tk_array) {
                  int idx = 0;
                  gcc_jit_type *idx_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  while (elem) {
                      gcc_jit_rvalue *elem_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(elem, &elem_rval));
                      gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_int(ctx, idx_type, idx);
                      gcc_jit_lvalue *arr_elem = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(temp_lval), idx_rval);
                      gcc_jit_block_add_assignment(cblock, loc, arr_elem, elem_rval);
                      idx++;
                      elem = elem->next;
                  }
              } else if (orig_type->kind == tk_class || orig_type->kind == tk_struct || orig_type->kind == tk_union) {
                  a_class_type_supplement_ptr ctsp = orig_type->variant.class_type.extra_info;
                  a_field_ptr fld = ctsp ? ctsp->field_list : NULL;
                  
                  while (elem && fld) {
                      gcc_jit_rvalue *elem_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(elem, &elem_rval));
                      
                      void *cached_field = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, fld, &cached_field);
                      if (err == GCC_GEN_BE_SUCCESS && cached_field) {
                          gcc_jit_lvalue *field_lval = gcc_jit_lvalue_access_field(temp_lval, NULL, (gcc_jit_field *)cached_field);
                          gcc_jit_block_add_assignment(cblock, loc, field_lval, elem_rval);
                      }
                      
                      elem = elem->next;
                      fld = fld->next;
                  }
              }
              
              *out_rval = gcc_jit_lvalue_as_rvalue(temp_lval);
              return GCC_GEN_BE_SUCCESS;
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

      case enk_condition:
          {
              a_condition_supplement_ptr cond = expr->variant.condition;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              
              if (cond->initialization) {
                  extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(cond->initialization, func));
              }
              
              if (cond->dynamic_init && cond->dynamic_init->kind != dik_none) {
                  a_dynamic_init_ptr dip = cond->dynamic_init;
                  if (dip->variable) {
                      void *cached = NULL;
                      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached));
                      gcc_jit_lvalue *var_lval = (gcc_jit_lvalue *)cached;
                      if (!var_lval) {
                          extern gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(gcc_jit_function *func, a_variable_ptr var, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, dip->variable, &var_lval));
                      }
                      
                      gcc_jit_rvalue *init_rval = NULL;
                      if (dip->kind == dik_zero) {
                          gcc_jit_type *type = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(dip->variable->type, &type));
                          init_rval = gcc_jit_context_zero(ctx, type);
                      } else if (dip->kind == dik_constant || dip->kind == dik_nonconstant_aggregate) {
                          gcc_jit_type *type = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(dip->variable->type, &type));
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(dip->variant.constant.ptr, type, &init_rval));
                      } else if (dip->kind == dik_expression || dip->kind == dik_class_result_via_ctor) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &init_rval));
                      }
                      
                      if (init_rval) {
                          gcc_jit_block_add_assignment(cblock, loc, var_lval, init_rval);
                      }
                      
                      if (dip->destructor) {
                          void *dtor_cached = NULL;
                          err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, dip->destructor, &dtor_cached);
                          if (err == GCC_GEN_BE_SUCCESS && dtor_cached) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(var_lval, (gcc_jit_function *)dtor_cached));
                          }
                      }
                  }
              }
              
              if (cond->expr) {
                  return gcc_gen_be_lower_expr_rvalue(cond->expr, out_rval);
              } else {
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 1);
                  return GCC_GEN_BE_SUCCESS;
              }
          }

      case enk_new_delete:
          {
              a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(ndsp->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              
              if (ndsp->is_new) {
                  gcc_jit_type *size_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                  gcc_jit_rvalue *alloc_size = NULL;
                  if (ndsp->number_of_elements) {
                      gcc_jit_rvalue *elem_size = gcc_jit_context_new_sizeof(ctx, type);
                      gcc_jit_rvalue *elem_size_cast = gcc_jit_context_new_cast(ctx, loc, elem_size, size_type);
                      gcc_jit_rvalue *num_elems = NULL;
                      err = gcc_gen_be_lower_expr_rvalue(ndsp->number_of_elements, &num_elems);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      gcc_jit_rvalue *num_elems_cast = gcc_jit_context_new_cast(ctx, loc, num_elems, size_type);
                      alloc_size = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, size_type, num_elems_cast, elem_size_cast);
                      
                      /* If an array cookie is explicitly required by ABI and not folded into __cxa_vec_new by frontend, we'd add it here. */
                  } else {
                      alloc_size = gcc_jit_context_new_sizeof(ctx, type);
                      alloc_size = gcc_jit_context_new_cast(ctx, loc, alloc_size, size_type);
                  }
                  
                  gcc_jit_rvalue *alloc_call = NULL;
                  if (ndsp->routine) {
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, ndsp->routine, &cached);
                      if (err == GCC_GEN_BE_SUCCESS && cached) {
                          gcc_jit_function *alloc_func = (gcc_jit_function *)cached;
                          
                          int num_args = 1;
                          an_expr_node_ptr arg = ndsp->arg;
                          while (arg) { num_args++; arg = arg->next; }
                          
                          gcc_jit_rvalue **args = (gcc_jit_rvalue **)malloc(num_args * sizeof(gcc_jit_rvalue *));
                          args[0] = alloc_size;
                          
                          arg = ndsp->arg;
                          int i = 1;
                          while (arg) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &args[i]));
                              i++;
                              arg = arg->next;
                          }
                          
                          alloc_call = gcc_jit_context_new_call(ctx, loc, alloc_func, num_args, args);
                          free(args);
                      }
                  } else {
                      /* Fallback to system malloc if routine is missing and not lowered */
                      gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                      gcc_jit_param *malloc_param = gcc_jit_context_new_param(ctx, NULL, size_type, "size");
                      gcc_jit_function *malloc_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "malloc", 1, &malloc_param, 0);
                      alloc_call = gcc_jit_context_new_call(ctx, loc, malloc_func, 1, &alloc_size);
                  }
                  
                  gcc_jit_type *ptr_type = gcc_jit_type_get_pointer(type);
                  gcc_jit_lvalue *allocated_ptr = gcc_jit_function_new_local(func, NULL, ptr_type, "new_ptr");
                  if (alloc_call) {
                      gcc_jit_block_add_assignment(cblock, loc, allocated_ptr, gcc_jit_context_new_cast(ctx, loc, alloc_call, ptr_type));
                  } else {
                      gcc_jit_block_add_assignment(cblock, loc, allocated_ptr, gcc_jit_context_null(ctx, ptr_type));
                  }
                  
                  if (ndsp->dynamic_init) {
                      /* Dereference the pointer to get the lvalue for the new object */
                      gcc_jit_lvalue *obj_lval = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(allocated_ptr), NULL);
                      
                      /* Evaluate initialization. A complete implementation would wrap this in a try/catch and call ndsp->freeing_of_storage_on_exception if it throws. */
                      // gcc_gen_be_lower_dynamic_init(ndsp->dynamic_init, obj_lval, type, cblock);
                  }
                  
                  *out_rval = gcc_jit_lvalue_as_rvalue(allocated_ptr);
              } else {
                  /* Delete */
                  gcc_jit_rvalue *ptr = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(ndsp->arg, &ptr));
                  
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
                  gcc_jit_rvalue *ptr_as_void = gcc_jit_context_new_cast(ctx, loc, ptr, void_ptr_type);
                  gcc_jit_rvalue *is_not_null = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, ptr_as_void, null_ptr);
                  
                  gcc_jit_block *delete_block = gcc_jit_function_new_block(func, "delete_not_null");
                  gcc_jit_block *end_block = gcc_jit_function_new_block(func, "delete_end");
                  
                  gcc_jit_block_end_with_conditional(cblock, loc, is_not_null, delete_block, end_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(delete_block));
                  
                  if (ndsp->dynamic_init) {
                      /* Destroy object(s) */
                      // gcc_gen_be_lower_dynamic_init(ndsp->dynamic_init, ..., type, delete_block);
                  }
                  
                  if (ndsp->routine) {
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, ndsp->routine, &cached);
                      if (err == GCC_GEN_BE_SUCCESS && cached) {
                          gcc_jit_function *dealloc_func = (gcc_jit_function *)cached;
                          gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, loc, dealloc_func, 1, &ptr);
                          gcc_jit_block_add_eval(delete_block, NULL, call);
                      }
                  } else {
                      /* Fallback to system free */
                      gcc_jit_param *free_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
                      gcc_jit_function *free_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "free", 1, &free_param, 0);
                      gcc_jit_block_add_eval(delete_block, NULL, gcc_jit_context_new_call(ctx, loc, free_func, 1, &ptr_as_void));
                  }
                  
                  gcc_jit_block_end_with_jump(delete_block, NULL, end_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
                  
                  *out_rval = gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
              }
              return GCC_GEN_BE_SUCCESS;
          }


          
      case enk_result_of_overriding_function:
          {
              a_routine_ptr wrapper_rout = innermost_function_scope ? innermost_function_scope->variant.routine.ptr : NULL;
              if (!wrapper_rout || !wrapper_rout->overriding_function_for_wrapper) {
                  return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
              }
              a_routine_ptr overriding_rout = wrapper_rout->overriding_function_for_wrapper;
              
              gcc_jit_function *func = NULL;
              err = gcc_gen_be_lower_function_decl(overriding_rout, &func);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              gcc_jit_function *current_jit_func = gcc_jit_block_get_function(cblock);

              int num_args = gcc_jit_function_get_param_count(current_jit_func);
              gcc_jit_rvalue **args = NULL;
              if (num_args > 0) {
                  args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                  
                  a_scope_ptr def_scope = scope_for_routine(wrapper_rout);
                  a_variable_ptr var_param = def_scope ? def_scope->variant.routine.parameters : NULL;
                  
                  for (int i = 0; i < num_args; i++) {
                      if (var_param) {
                          void *cached_lval = NULL;
                          err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var_param, &cached_lval);
                          if (err == GCC_GEN_BE_SUCCESS && cached_lval) {
                              args[i] = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_lval);
                          } else {
                              gcc_jit_param *param = gcc_jit_function_get_param(current_jit_func, i);
                              args[i] = gcc_jit_param_as_rvalue(param);
                          }
                          var_param = var_param->next;
                      } else {
                          gcc_jit_param *param = gcc_jit_function_get_param(current_jit_func, i);
                          args[i] = gcc_jit_param_as_rvalue(param);
                      }
                  }
              }
              
              gcc_jit_type *expected_type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &expected_type);
              if (err != GCC_GEN_BE_SUCCESS) {
                  if (args) free(args);
                  return err;
              }
              
              gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, loc, func, num_args, args);
              if (args) free(args);
              
              *out_rval = gcc_jit_context_new_cast(ctx, loc, call_rval, expected_type);
              return GCC_GEN_BE_SUCCESS;
          }

          
      case enk_throw:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *out_block = NULL;
              err = gcc_gen_be_emit_throw(expr, &out_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
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