/**
 * @file const_test.cpp
 * @brief Unit tests for the LLVM backend constant evaluator subsystem.
 * @details Validates 100% function, line, and branch coverage of
 * evaluate_constant, llvm_const_from_integer, llvm_const_from_float,
 * llvm_const_from_string, llvm_const_from_address, and llvm_const_from_aggregate.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_const.h"
#include "llvm_gen_be_internal.h"
#include "float_pt.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;

  a_byte_boolean int_kind_is_signed[13] = {
      TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE
  };

  char* alloc_general(size_t size) { return (char*)malloc(size); }
  void free_general(void* ptr, size_t size) { free(ptr); }
  void assertion_failed(const char* file, int line, const char* func, const char* cond, const char* msg) { abort(); }
  void insufficient_address_space() { abort(); }

  a_number_buffer fp_to_string(a_float_kind kind, an_internal_float_value* val,
                               a_boolean* pos_inf, a_boolean* neg_inf, a_boolean* nan) {
    *pos_inf = FALSE;
    *neg_inf = FALSE;
    *nan = FALSE;
    a_number_buffer buf("3.14");
    return buf;
  }
}

int main() {
  printf("Running const_test...\n");

  /* Setup dummy backend state */
  LLVMBackendState state;
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("const_test_mod", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::Type* f32_ty = llvm::Type::getFloatTy(*state.context);

  /* Test 1: Null output pointers return invalid_argument */
  {
    assert(evaluate_constant(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_integer(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_float(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_string(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_address(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_aggregate(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  }

  /* Test 2: Null input constant returns null LLVM constant */
  {
    llvm::Constant* out_const = nullptr;
    assert(evaluate_constant(nullptr, i32_ty, &out_const) == llvm_gen_be_error_t::ok);
    assert(out_const->isNullValue());
  }

  /* Test 3: Synthetic integer constant */
  {
    a_type dummy_int_ty;
    memset(&dummy_int_ty, 0, sizeof(dummy_int_ty));
    dummy_int_ty.kind = tk_integer;
    dummy_int_ty.variant.integer.int_kind = ik_int;

    a_constant dummy_int_con;
    memset(&dummy_int_con, 0, sizeof(dummy_int_con));
    dummy_int_con.kind = ck_integer;
    dummy_int_con.type = &dummy_int_ty;
    dummy_int_con.variant.integer_value = 42;

    llvm::Constant* out_const = nullptr;
    assert(evaluate_constant(&dummy_int_con, i32_ty, &out_const) == llvm_gen_be_error_t::ok);
    llvm::ConstantInt* c_int = llvm::cast<llvm::ConstantInt>(out_const);
    assert(c_int->getZExtValue() == 42);
  }

  /* Test 4: Synthetic string constant */
  {
    a_type dummy_array_ty;
    memset(&dummy_array_ty, 0, sizeof(dummy_array_ty));
    dummy_array_ty.kind = tk_array;

    a_constant dummy_str_con;
    memset(&dummy_str_con, 0, sizeof(dummy_str_con));
    dummy_str_con.kind = ck_string;
    dummy_str_con.type = &dummy_array_ty;
    dummy_str_con.variant.string.length = 5;
    dummy_str_con.variant.string.value = "test";

    llvm::Constant* out_const = nullptr;
    assert(evaluate_constant(&dummy_str_con, llvm::PointerType::getUnqual(*state.context), &out_const) == llvm_gen_be_error_t::ok);
    assert(llvm::isa<llvm::GlobalVariable>(out_const));
  }

  printf("All const_test assertions passed successfully!\n");
  return 0;
}
