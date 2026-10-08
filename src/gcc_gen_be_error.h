/**
 * @file gcc_gen_be_error.h
 * @brief Centralized error handling for the GCC code generation backend.
 *
 * This file defines the error enumeration and utility macros used throughout
 * the GCC backend to propagate errors and ensure return values are not ignored.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_ERROR_H
#define GCC_GEN_BE_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro to enforce that the return value of a function is not ignored.
 *
 * Applies the `warn_unused_result` attribute to ensure that functions returning
 * an error code are properly checked by the caller.
 */
#if defined(__GNUC__) || defined(__clang__)
#define GCC_GEN_BE_NODISCARD __attribute__((warn_unused_result))
#else
#define GCC_GEN_BE_NODISCARD
#endif

/**
 * @enum gcc_gen_be_error_t
 * @brief Represents the possible error codes returned by the GCC backend.
 *
 * This enumeration defines the standardized error codes that must be returned
 * by all functions in the backend to indicate success or the specific type
 * of failure encountered.
 */
typedef enum {
    /** @brief Indicates that the operation completed successfully. */
    GCC_GEN_BE_SUCCESS = 0,

    /** @brief Indicates that a memory allocation failed (Out Of Memory). */
    GCC_GEN_BE_ERROR_OOM,

    /** @brief Indicates that an unsupported operation or language feature was encountered. */
    GCC_GEN_BE_ERROR_UNSUPPORTED,

    /** @brief Indicates that the libgccjit dynamic library could not be loaded. */
    GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED,

    /** @brief Indicates that a required symbol could not be found in libgccjit. */
    GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING,

    /** @brief Indicates an invalid argument was passed to a function. */
    GCC_GEN_BE_ERROR_INVALID_ARGUMENT,

    /** @brief Indicates an internal compiler error within the backend. */
    GCC_GEN_BE_ERROR_INTERNAL,

    /** @brief Indicates that a specific type could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_TYPE,

    /** @brief Indicates that a specific expression could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_EXPR,

    /** @brief Indicates that a specific statement could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_STMT,

    /** @brief Indicates that a specific declaration could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_DECL,

    /** @brief Indicates that a required pointer was null. */
    GCC_GEN_BE_ERROR_NULL_POINTER,

    /** @brief Indicates a type mismatch during code generation. */
    GCC_GEN_BE_ERROR_TYPE_MISMATCH,

    /** @brief Indicates a failure in exception handling setup or generation. */
    GCC_GEN_BE_ERROR_EH_FAILURE
} gcc_gen_be_error_t;

/**
 * @brief Macro to invoke an expression and return immediately if it fails.
 *
 * This macro evaluates the given expression, which must return a `gcc_gen_be_error_t`.
 * If the returned value is not `GCC_GEN_BE_SUCCESS`, the macro immediately
 * returns that error code from the current function.
 *
 * @param expr The expression to evaluate.
 */
#define GCC_GEN_BE_CHECK(expr) \
    do { \
        gcc_gen_be_error_t _err = (expr); \
        if (_err != GCC_GEN_BE_SUCCESS) { \
            return _err; \
        } \
    } while (0)

/**
 * @brief Converts a GCC backend error code to a human-readable string.
 *
 * @param error The error code to convert.
 * @return A constant character string representing the error code.
 */
extern GCC_GEN_BE_NODISCARD const char *gcc_gen_be_error_string(gcc_gen_be_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_ERROR_H */