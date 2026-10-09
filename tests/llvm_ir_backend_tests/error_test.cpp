/**
 * @file error_test.cpp
 * @brief Unit tests for the LLVM backend centralized error subsystem.
 * @details Validates 100% function, line, and branch coverage of
 * llvm_gen_be_set_error, llvm_gen_be_error_to_string, and llvm_gen_be_error_context_reset.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_error.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace edg;

/**
 * @brief Test runner validating all branches of the error handling subsystem.
 * @details Executes all assertions across error enum variants, string formatting,
 * null-checking, and context resets.
 * @return 0 on success.
 */
int main() {
  printf("Running error_test...\n");

  /* Test 1: llvm_gen_be_error_to_string null check */
  {
    llvm_gen_be_error_t res = llvm_gen_be_error_to_string(llvm_gen_be_error_t::ok, nullptr);
    assert(res == llvm_gen_be_error_t::invalid_argument);
  }

  /* Test 2: llvm_gen_be_error_to_string all enumerators + default */
  {
    const char* str = nullptr;

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::ok, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "ok") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::invalid_argument, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "invalid_argument") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::out_of_memory, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "out_of_memory") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::unsupported_type, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "unsupported_type") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::unsupported_expr, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "unsupported_expr") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::unsupported_stmt, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "unsupported_stmt") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::abi_classification_failed, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "abi_classification_failed") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::di_metadata_failure, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "di_metadata_failure") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::pass_pipeline_failure, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "pass_pipeline_failure") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::code_gen_failure, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "code_gen_failure") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::verification_failure, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "verification_failure") == 0);

    assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::io_error, &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "io_error") == 0);

    /* Test unknown / default branch */
    assert(llvm_gen_be_error_to_string(static_cast<llvm_gen_be_error_t>(999), &str) == llvm_gen_be_error_t::ok);
    assert(strcmp(str, "unknown_error") == 0);
  }

  /* Test 3: llvm_gen_be_set_error with null ctx */
  {
    llvm_gen_be_error_t res = llvm_gen_be_set_error(
        nullptr,
        llvm_gen_be_error_t::invalid_argument,
        "test.c",
        10,
        5,
        "Error %d",
        42);
    assert(res == llvm_gen_be_error_t::invalid_argument);
  }

  /* Test 4: llvm_gen_be_set_error with valid ctx and format string */
  {
    llvm_gen_be_error_context_t ctx;
    llvm_gen_be_error_t res = llvm_gen_be_set_error(
        &ctx,
        llvm_gen_be_error_t::unsupported_type,
        "types.c",
        42,
        8,
        "Type index %d unrecognized",
        123);
    assert(res == llvm_gen_be_error_t::unsupported_type);
    assert(ctx.error_code == llvm_gen_be_error_t::unsupported_type);
    assert(strcmp(ctx.file_name, "types.c") == 0);
    assert(ctx.line_number == 42);
    assert(ctx.column_number == 8);
    assert(strcmp(ctx.message, "Type index 123 unrecognized") == 0);
  }

  /* Test 5: llvm_gen_be_set_error with nullptr format string */
  {
    llvm_gen_be_error_context_t ctx;
    llvm_gen_be_error_t res = llvm_gen_be_set_error(
        &ctx,
        llvm_gen_be_error_t::out_of_memory,
        "alloc.c",
        100,
        1,
        nullptr);
    assert(res == llvm_gen_be_error_t::out_of_memory);
    assert(ctx.error_code == llvm_gen_be_error_t::out_of_memory);
    assert(strcmp(ctx.file_name, "alloc.c") == 0);
    assert(ctx.line_number == 100);
    assert(ctx.column_number == 1);
    assert(ctx.message[0] == '\0');
  }

  /* Test 6: llvm_gen_be_error_context_reset */
  {
    /* Null context check */
    assert(llvm_gen_be_error_context_reset(nullptr) == llvm_gen_be_error_t::invalid_argument);

    /* Valid reset */
    llvm_gen_be_error_context_t ctx;
    assert(llvm_gen_be_set_error(&ctx, llvm_gen_be_error_t::io_error, "file.c", 5, 2, "IO Error") == llvm_gen_be_error_t::io_error);
    assert(ctx.error_code == llvm_gen_be_error_t::io_error);
    assert(llvm_gen_be_error_context_reset(&ctx) == llvm_gen_be_error_t::ok);
    assert(ctx.error_code == llvm_gen_be_error_t::ok);
    assert(ctx.file_name == nullptr);
    assert(ctx.line_number == 0);
    assert(ctx.column_number == 0);
    assert(ctx.message[0] == '\0');
  }

  printf("All error_test assertions passed successfully!\n");
  return 0;
}
