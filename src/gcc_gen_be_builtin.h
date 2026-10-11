/**
 * @file gcc_gen_be_builtin.h
 * @brief Builtin and Intrinsic lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_BUILTIN_H
#define GCC_GEN_BE_BUILTIN_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a call to a compiler builtin or intrinsic.
 *
 * @param rout The routine entry of the builtin.
 * @param num_args The number of arguments in the call.
 * @param args Array of JIT rvalues for the arguments.
 * @param out_rval Pointer to receive the generated JIT rvalue for the result.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_BUILTIN_H */
