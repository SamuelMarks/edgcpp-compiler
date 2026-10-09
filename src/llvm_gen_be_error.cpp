/**
 * @file llvm_gen_be_error.cpp
 * @brief Implementation of error reporting and conversion functions for LLVM backend.
 * @details Implements llvm_gen_be_set_error, llvm_gen_be_error_to_string, and
 * llvm_gen_be_error_context_reset.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_error.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

BEGIN_EDG_NAMESPACE

/**
 * @brief Sets and records error details within an error context.
 * @details Formats a diagnostic message into the error context structure and
 * sets the corresponding error code and location information.
 * @param[in,out] ctx Pointer to the error context to populate. If null, the function
 *                    still returns the error code without recording message details.
 * @param[in] err The error code representing the failure.
 * @param[in] file_name Source file path where the error originated, or nullptr.
 * @param[in] line Source line number where the error originated, or 0.
 * @param[in] col Source column number where the error originated, or 0.
 * @param[in] format Printf-style format string for the error message, or nullptr.
 * @param[in] ... Variadic arguments matching the format string.
 * @return The error code passed in @p err.
 */
llvm_gen_be_error_t llvm_gen_be_set_error(
    llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t err,
    const char* file_name,
    uint32_t line,
    uint32_t col,
    const char* format,
    ...) noexcept {
  if (ctx != nullptr) {
    ctx->error_code = err;
    ctx->file_name = file_name;
    ctx->line_number = line;
    ctx->column_number = col;
    if (format != nullptr) {
      va_list args;
      va_start(args, format);
      vsnprintf(ctx->message, sizeof(ctx->message), format, args);
      va_end(args);
    } else {
      ctx->message[0] = '\0';
    }
  }
  return err;
}

/**
 * @brief Converts an error code into a human-readable string representation.
 * @details Translates each enumeration value into a constant C-string name.
 * @param[in] err The error code to convert.
 * @param[out] out_str Pointer to a const char* receive parameter where the
 *                     resulting string pointer is stored.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_str is null.
 */
llvm_gen_be_error_t llvm_gen_be_error_to_string(
    llvm_gen_be_error_t err,
    const char** out_str) noexcept {
  if (out_str == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  switch (err) {
    case llvm_gen_be_error_t::ok:
      *out_str = "ok";
      break;
    case llvm_gen_be_error_t::invalid_argument:
      *out_str = "invalid_argument";
      break;
    case llvm_gen_be_error_t::out_of_memory:
      *out_str = "out_of_memory";
      break;
    case llvm_gen_be_error_t::unsupported_type:
      *out_str = "unsupported_type";
      break;
    case llvm_gen_be_error_t::unsupported_expr:
      *out_str = "unsupported_expr";
      break;
    case llvm_gen_be_error_t::unsupported_stmt:
      *out_str = "unsupported_stmt";
      break;
    case llvm_gen_be_error_t::abi_classification_failed:
      *out_str = "abi_classification_failed";
      break;
    case llvm_gen_be_error_t::di_metadata_failure:
      *out_str = "di_metadata_failure";
      break;
    case llvm_gen_be_error_t::pass_pipeline_failure:
      *out_str = "pass_pipeline_failure";
      break;
    case llvm_gen_be_error_t::code_gen_failure:
      *out_str = "code_gen_failure";
      break;
    case llvm_gen_be_error_t::verification_failure:
      *out_str = "verification_failure";
      break;
    case llvm_gen_be_error_t::io_error:
      *out_str = "io_error";
      break;
    default:
      *out_str = "unknown_error";
      break;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resets an error context structure to default success state.
 * @details Clears the error code to ok, resets line and column to 0, and clears the message buffer.
 * @param[in,out] ctx Pointer to the error context to reset.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx is null.
 */
llvm_gen_be_error_t llvm_gen_be_error_context_reset(
    llvm_gen_be_error_context_t* ctx) noexcept {
  if (ctx == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  ctx->error_code = llvm_gen_be_error_t::ok;
  ctx->file_name = nullptr;
  ctx->line_number = 0;
  ctx->column_number = 0;
  ctx->message[0] = '\0';
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
