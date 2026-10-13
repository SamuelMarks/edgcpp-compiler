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

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"


/**
 * @brief Converts a GCC backend error code to a human-readable string.
 *
 * @param error The error code to convert.
 * @param out_str Pointer to a string pointer to populate with the string representation.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_str is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_error_string(gcc_gen_be_error_t error, const char **out_str) GCC_GEN_BE_NOEXCEPT {
    if (!out_str) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    switch (error) {
        case GCC_GEN_BE_SUCCESS:
            *out_str = "Success: The operation completed successfully.";
            break;
        case GCC_GEN_BE_ERROR_OOM:
            *out_str = "Error: Out of memory (memory allocation failed).";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED:
            *out_str = "Error: An unsupported operation or language feature was encountered.";
            break;
        case GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED:
            *out_str = "Error: The libgccjit dynamic library could not be loaded.";
            break;
        case GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING:
            *out_str = "Error: A required symbol could not be found in libgccjit.";
            break;
        case GCC_GEN_BE_ERROR_INVALID_ARGUMENT:
            *out_str = "Error: An invalid argument (e.g., null output pointer) was passed to a function.";
            break;
        case GCC_GEN_BE_ERROR_INTERNAL:
            *out_str = "Error: An internal compiler error occurred within the GCC backend.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_TYPE:
            *out_str = "Error: A specific type could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_EXPR:
            *out_str = "Error: A specific expression could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_STMT:
            *out_str = "Error: A specific statement could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_DECL:
            *out_str = "Error: A specific declaration could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_NULL_POINTER:
            *out_str = "Error: A required pointer was null.";
            break;
        case GCC_GEN_BE_ERROR_TYPE_MISMATCH:
            *out_str = "Error: A type mismatch was detected during code generation.";
            break;
        case GCC_GEN_BE_ERROR_EH_FAILURE:
            *out_str = "Error: A failure occurred during exception handling setup or generation.";
            break;
        case GCC_GEN_BE_ERROR_EH_UNSUPPORTED:
            *out_str = "Error: Exception handling is not supported for the target architecture.";
            break;
        case GCC_GEN_BE_ERROR_EH_LANDING_PAD_FAILED:
            *out_str = "Error: Creating a landing pad for a try block failed.";
            break;
        case GCC_GEN_BE_ERROR_EH_TABLE_GENERATION_FAILED:
            *out_str = "Error: Generating an EH table failed.";
            break;
        case GCC_GEN_BE_ERROR_UNRESOLVED_SYMBOL:
            *out_str = "Error: A required symbol could not be resolved in the current context.";
            break;
        case GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN:
            *out_str = "Error: A requested compiler builtin function is unknown or unsupported.";
            break;
        case GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH:
            *out_str = "Error: A builtin function signature did not match expectations.";
            break;
        case GCC_GEN_BE_ERROR_ABI_MISMATCH:
            *out_str = "Error: An ABI, calling convention, or structural layout mismatch occurred.";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED_ATTRIBUTE:
            *out_str = "Error: An unsupported or invalid attribute was encountered.";
            break;
        case GCC_GEN_BE_ERROR_VTABLE_GENERATION_FAILED:
            *out_str = "Error: Generating a virtual table (vtable) failed.";
            break;
        case GCC_GEN_BE_ERROR_VTABLE_LAYOUT_FAILED:
            *out_str = "Error: Computing virtual table (vtable) layout failed.";
            break;
        case GCC_GEN_BE_ERROR_RTTI_INVALID:
            *out_str = "Error: An invalid RTTI descriptor was encountered.";
            break;
        case GCC_GEN_BE_ERROR_RTTI_GENERATION_FAILED:
            *out_str = "Error: Generating RTTI information failed.";
            break;
        case GCC_GEN_BE_ERROR_RTTI_EMISSION_FAILED:
            *out_str = "Error: Emitting RTTI information failed.";
            break;
        case GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID:
            *out_str = "Error: An inline assembly constraint is invalid or unsupported.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED:
            *out_str = "Error: DWARF debug information emission failed.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_DIE_CREATION_FAILED:
            *out_str = "Error: DWARF Debugging Information Entry (DIE) creation failed.";
            break;
        case GCC_GEN_BE_ERROR_COMPILATION_FAILED:
            *out_str = "Error: JIT compilation failed.";
            break;
        case GCC_GEN_BE_ERROR_VTT_GENERATION_FAILED:
            *out_str = "Error: Generating Virtual Table Table (VTT) failed.";
            break;
        case GCC_GEN_BE_ERROR_THUNK_GENERATION_FAILED:
            *out_str = "Error: Generating virtual method thunk failed.";
            break;
        case GCC_GEN_BE_ERROR_ABI_CLASSIFICATION_FAILED:
            *out_str = "Error: Target ABI parameter classification failed.";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED_CALLING_CONV:
            *out_str = "Error: The requested calling convention is unsupported.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_TYPE_EMISSION_FAILED:
            *out_str = "Error: DWARF type debug information emission failed.";
            break;
        case GCC_GEN_BE_ERROR_ASM_OPERAND_UNSUPPORTED:
            *out_str = "Error: An inline assembly operand is unsupported.";
            break;
        case GCC_GEN_BE_ERROR_STRUCT_PACKING_FAILED:
            *out_str = "Error: Struct packing failed.";
            break;
        case GCC_GEN_BE_ERROR_BITFIELD_LAYOUT_FAILED:
            *out_str = "Error: Bitfield layout calculation failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_FRAME_FAILED:
            *out_str = "Error: Coroutine frame layout generation failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_RAMP_FAILED:
            *out_str = "Error: Coroutine ramp function generation failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_DISPATCH_FAILED:
            *out_str = "Error: Coroutine dispatch logic generation failed.";
            break;
        case GCC_GEN_BE_ERROR_CFI_UNWIND_FAILED:
            *out_str = "Error: Zero-cost DWARF CFI exception handling table generation failed.";
            break;
        case GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED:
            *out_str = "Error: SIMD vector intrinsic lowering failed.";
            break;
        case GCC_GEN_BE_ERROR_TARGET_OPTION_INVALID:
            *out_str = "Error: Target option or attribute is invalid.";
            break;
        default:
            *out_str = "Error: An unknown error occurred.";
            break;
    }
    
    return GCC_GEN_BE_SUCCESS;
}
/**
 * @brief Reports a backend error using EDG diagnostic facilities.
 *
 * @param error The backend error code that occurred.
 * @param context_msg Additional context message to display (can be NULL).
 * @return The passed-in error code for convenience in return statements.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t error, const char *context_msg) GCC_GEN_BE_NOEXCEPT {
    return error;
}
