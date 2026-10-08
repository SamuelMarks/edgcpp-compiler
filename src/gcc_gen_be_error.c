/**
 * @file gcc_gen_be_error.c
 * @brief Implementation of error handling utilities for the GCC backend.
 *
 * This file provides the implementation for functions defined in gcc_gen_be_error.h,
 * primarily for converting error codes into human-readable strings.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_error.h"

/**
 * @brief Converts a GCC backend error code to a human-readable string.
 *
 * @param error The error code to convert.
 * @return A constant character string representing the error code. If the error
 *         code is unknown, returns "GCC_GEN_BE_ERROR_UNKNOWN".
 */
const char *gcc_gen_be_error_string(gcc_gen_be_error_t error) {
    switch (error) {
        case GCC_GEN_BE_SUCCESS:
            return "GCC_GEN_BE_SUCCESS";
        case GCC_GEN_BE_ERROR_OOM:
            return "GCC_GEN_BE_ERROR_OOM";
        case GCC_GEN_BE_ERROR_UNSUPPORTED:
            return "GCC_GEN_BE_ERROR_UNSUPPORTED";
        case GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED:
            return "GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED";
        case GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING:
            return "GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING";
        case GCC_GEN_BE_ERROR_INVALID_ARGUMENT:
            return "GCC_GEN_BE_ERROR_INVALID_ARGUMENT";
        case GCC_GEN_BE_ERROR_INTERNAL:
            return "GCC_GEN_BE_ERROR_INTERNAL";
        case GCC_GEN_BE_ERROR_UNHANDLED_TYPE:
            return "GCC_GEN_BE_ERROR_UNHANDLED_TYPE";
        case GCC_GEN_BE_ERROR_UNHANDLED_EXPR:
            return "GCC_GEN_BE_ERROR_UNHANDLED_EXPR";
        case GCC_GEN_BE_ERROR_UNHANDLED_STMT:
            return "GCC_GEN_BE_ERROR_UNHANDLED_STMT";
        case GCC_GEN_BE_ERROR_UNHANDLED_DECL:
            return "GCC_GEN_BE_ERROR_UNHANDLED_DECL";
        case GCC_GEN_BE_ERROR_NULL_POINTER:
            return "GCC_GEN_BE_ERROR_NULL_POINTER";
        case GCC_GEN_BE_ERROR_TYPE_MISMATCH:
            return "GCC_GEN_BE_ERROR_TYPE_MISMATCH";
        case GCC_GEN_BE_ERROR_EH_FAILURE:
            return "GCC_GEN_BE_ERROR_EH_FAILURE";
        default:
            return "GCC_GEN_BE_ERROR_UNKNOWN";
    }
}