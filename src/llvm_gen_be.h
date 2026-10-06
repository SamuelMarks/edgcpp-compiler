/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

llvm_gen_be.h - Declarations related to llvm_gen_be.cpp (LLVM IR-generating back end)

*/

/* Avoid including these declarations more than once: */
#ifndef LLVM_GEN_BE_H
#define LLVM_GEN_BE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#if BACK_END_IS_LLVM_GEN_BE

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM
extern void back_end(void);
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE
extern void llvm_gen_be_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */

#endif /* ifndef LLVM_GEN_BE_H */