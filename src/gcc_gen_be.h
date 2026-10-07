/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef GCC_GEN_BE_H
#define GCC_GEN_BE_H

#include "basic_hdrs.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

extern void gcc_gen_be_early_init(void);
extern void gcc_gen_be(void);
extern void back_end(void);

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */

#endif /* GCC_GEN_BE_H */
