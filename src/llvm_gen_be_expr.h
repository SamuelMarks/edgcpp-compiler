/**
 * @file llvm_gen_be_expr.h
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*). All functions return llvm_gen_be_error_t and output
 * constructed values via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_EXPR_H
#define LLVM_GEN_BE_EXPR_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Value.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG expression into an LLVM Value.
 * @details Traverses the EDG expression tree and emits corresponding LLVM IR instructions.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val);

/**
 * @brief Lowers an LValue expression.
 * @details Emits instructions to compute the address of an LValue expression.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_ptr Pointer to the variable where the resulting llvm::Value* (pointer) is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr);

/**
 * @brief Lowers unary and binary arithmetic expressions.
 * @details Handles +, -, *, /, %, etc.
 * @param[in] expr Pointer to the EDG arithmetic expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val);

/**
 * @brief Lowers comparison and logical short-circuit expressions.
 * @details Handles ==, !=, <, >, &&, ||, etc.
 * @param[in] expr Pointer to the EDG comparison/logical expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val);

/**
 * @brief Lowers type cast and conversion expressions.
 * @details Handles implicit and explicit casts.
 * @param[in] expr Pointer to the EDG cast expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val);

/**
 * @brief Lowers function call and invoke expressions.
 * @details Emits call or invoke instructions with argument lowering.
 * @param[in] expr Pointer to the EDG call expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val);

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_EXPR_H */
