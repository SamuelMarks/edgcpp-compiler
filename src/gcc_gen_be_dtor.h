/**
 * @file gcc_gen_be_dtor.h
 * @brief Global constructors and destructors for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_DTOR_H
#define GCC_GEN_BE_DTOR_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Emits a call to __cxa_atexit to register a global destructor.
 *
 * @param var The global variable.
 * @param dtor_func The destructor function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_cxa_atexit(gcc_jit_lvalue *var, gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a call to __cxa_thread_atexit to register a thread-local destructor.
 *
 * @param var The thread-local variable.
 * @param dtor_func The destructor function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_cxa_thread_atexit(gcc_jit_lvalue *var, gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_DTOR_H */
