/**
 * @file fault_injection_test.cpp
 * @brief Fault-Injection and Negative Test Suite for the LLVM Backend.
 * @details Validates error bubbling and context population for every error
 * variant by mocking invalid inputs, unhandled IL nodes, and IO failures.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_error.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_expr.h"
#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_internal.h"
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
  void internal_error(char const *) {}
}

int main() {
  printf("Running fault_injection_test...\n");

  /* Setup dummy backend state */
  LLVMBackendState state;
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("fault_injection_mod", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  /* 1. Test unsupported_type */
  {
    a_type dummy_unknown;
    memset(&dummy_unknown, 0, sizeof(dummy_unknown));
    dummy_unknown.kind = static_cast<a_type_kind>(999);

    llvm::Type* ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(&dummy_unknown, &ty);
    assert(err == llvm_gen_be_error_t::unsupported_type);
  }

  /* 2. Test unsupported_expr */
  {
    an_expr_node dummy_expr;
    memset(&dummy_expr, 0, sizeof(dummy_expr));
    dummy_expr.kind = static_cast<an_expr_node_kind>(999);

    llvm::Value* val = nullptr;
    llvm_gen_be_error_t err = lower_expression(&dummy_expr, &val);
    assert(err == llvm_gen_be_error_t::unsupported_expr);
  }

  /* 3. Test io_error */
  {
    llvm::TargetMachine* tm = nullptr;
    std::string default_triple = llvm::sys::getDefaultTargetTriple();
    create_target_machine(default_triple.c_str(), nullptr, nullptr, llvm::CodeGenOptLevel::None, &tm);
    
    llvm_gen_be_error_t err = emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::object_file, "/invalid/path/to/mock/io/failure.o");
    assert(err == llvm_gen_be_error_t::io_error);
    delete tm;
  }

  /* 4. Test invalid_argument */
  {
    llvm::Value* val = nullptr;
    llvm_gen_be_error_t err = lower_expression(nullptr, &val);
    assert(err == llvm_gen_be_error_t::invalid_argument);
  }

  /* 5. Exercise all error codes via llvm_gen_be_set_error explicitly
     to ensure context population doesn't abort. */
  llvm_gen_be_error_t all_errors[] = {
      llvm_gen_be_error_t::ok,
      llvm_gen_be_error_t::invalid_argument,
      llvm_gen_be_error_t::out_of_memory,
      llvm_gen_be_error_t::unsupported_type,
      llvm_gen_be_error_t::unsupported_expr,
      llvm_gen_be_error_t::unsupported_stmt,
      llvm_gen_be_error_t::abi_classification_failed,
      llvm_gen_be_error_t::di_metadata_failure,
      llvm_gen_be_error_t::pass_pipeline_failure,
      llvm_gen_be_error_t::code_gen_failure,
      llvm_gen_be_error_t::verification_failure,
      llvm_gen_be_error_t::io_error
  };

  for (auto err_code : all_errors) {
      llvm_gen_be_error_context_t ctx;
      llvm_gen_be_error_context_reset(&ctx);
      llvm_gen_be_error_t res = llvm_gen_be_set_error(&ctx, err_code, "mock_file.c", 10, 5, "Mock error message");
      assert(res == err_code);
      if (err_code != llvm_gen_be_error_t::ok) {
          assert(ctx.error_code == err_code);
          assert(strcmp(ctx.file_name, "mock_file.c") == 0);
          assert(ctx.line_number == 10);
          assert(ctx.column_number == 5);
          assert(strcmp(ctx.message, "Mock error message") == 0);
      }
  }

  printf("All fault_injection_test assertions passed successfully!\n");
  return 0;
}
