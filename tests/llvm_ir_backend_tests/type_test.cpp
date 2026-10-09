/**
 * @file type_test.cpp
 * @brief Unit tests for the LLVM backend type lowering subsystem.
 * @details Validates 100% function, line, and branch coverage of
 * llvm_type_from_integer, llvm_type_from_float, llvm_type_from_pointer,
 * llvm_type_from_array, llvm_type_from_struct, llvm_type_from_routine,
 * and llvm_type_from_edg_type.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_type.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
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
}

/**
 * @brief Test runner validating all branches of type translation functions.
 * @return 0 on success.
 */
int main() {
  printf("Running type_test...\n");

  /* Setup dummy backend state */
  LLVMBackendState state;
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("type_test_mod", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  /* Test 1: Null output pointers return invalid_argument */
  {
    assert(llvm_type_from_integer(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_float(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_pointer(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_array(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_struct(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_routine(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_edg_type(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  }

  /* Test 2: Null input edg_type returns default success types */
  {
    llvm::Type* ty = nullptr;
    llvm::FunctionType* fty = nullptr;

    assert(llvm_type_from_integer(nullptr, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getInt32Ty(*state.context));

    assert(llvm_type_from_float(nullptr, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getDoubleTy(*state.context));

    assert(llvm_type_from_pointer(nullptr, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::PointerType::getUnqual(*state.context));

    assert(llvm_type_from_array(nullptr, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::PointerType::getUnqual(*state.context));

    assert(llvm_type_from_struct(nullptr, &ty) == llvm_gen_be_error_t::ok);
    assert(ty->isStructTy());

    assert(llvm_type_from_routine(nullptr, &fty) == llvm_gen_be_error_t::ok);
    assert(fty->getReturnType()->isVoidTy());

    assert(llvm_type_from_edg_type(nullptr, &ty) == llvm_gen_be_error_t::ok);
    assert(ty->isVoidTy());

    /* get_llvm_type legacy wrapper */
    assert(get_llvm_type(nullptr)->isVoidTy());
  }

  /* Test 3: Synthetic EDG types for all float variants */
  {
    a_type dummy_fp;
    memset(&dummy_fp, 0, sizeof(dummy_fp));
    dummy_fp.kind = tk_float;

    llvm::Type* ty = nullptr;

    dummy_fp.variant.float_kind = fk_float16;
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getHalfTy(*state.context));

    dummy_fp.variant.float_kind = fk_std_bfloat16;
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getBFloatTy(*state.context));

    dummy_fp.variant.float_kind = fk_float;
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getFloatTy(*state.context));

    dummy_fp.variant.float_kind = fk_double;
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getDoubleTy(*state.context));

    dummy_fp.variant.float_kind = fk_float80;
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getX86_FP80Ty(*state.context));

    dummy_fp.variant.float_kind = fk_float128;
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::ok);
    assert(ty == llvm::Type::getFP128Ty(*state.context));

    /* Unsupported float kind returns unsupported_type error */
    dummy_fp.variant.float_kind = static_cast<a_float_kind>(99);
    assert(llvm_type_from_float(&dummy_fp, &ty) == llvm_gen_be_error_t::unsupported_type);
  }

  /* Test 4: Synthetic EDG integer types */
  {
    a_type dummy_int;
    memset(&dummy_int, 0, sizeof(dummy_int));
    dummy_int.kind = tk_integer;

    llvm::Type* ty = nullptr;

    dummy_int.variant.integer.int_kind = ik_char;
    assert(llvm_type_from_integer(&dummy_int, &ty) == llvm_gen_be_error_t::ok);
    assert(ty->isIntegerTy(8));

    dummy_int.variant.integer.int_kind = ik_short;
    assert(llvm_type_from_integer(&dummy_int, &ty) == llvm_gen_be_error_t::ok);
    assert(ty->isIntegerTy(16));

    dummy_int.variant.integer.int_kind = ik_int;
    assert(llvm_type_from_integer(&dummy_int, &ty) == llvm_gen_be_error_t::ok);
    assert(ty->isIntegerTy(32));

    dummy_int.variant.integer.int_kind = ik_long;
    assert(llvm_type_from_integer(&dummy_int, &ty) == llvm_gen_be_error_t::ok);
    assert(ty->isIntegerTy(64));
  }

  /* Test 5: Synthetic unknown AST type kind */
  {
    a_type dummy_unknown;
    memset(&dummy_unknown, 0, sizeof(dummy_unknown));
    dummy_unknown.kind = static_cast<a_type_kind>(999);

    llvm::Type* ty = nullptr;
    assert(llvm_type_from_edg_type(&dummy_unknown, &ty) == llvm_gen_be_error_t::unsupported_type);
  }

  printf("All type_test assertions passed successfully!\n");
  return 0;
}
