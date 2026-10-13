/**
 * @file gcc_gen_be_location.c
 * @brief Implementation of Source Location Tracking for DWARF Generation.
 *
 * Maps EDG IL source coordinates (line, column, filename) to gcc_jit_location
 * to enable precise DWARF metadata emission.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_location.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "il_def.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Maps a source correspondence to a gcc_jit_location.
 *
 * @param sc The source correspondence.
 * @param out_loc Pointer to receive the location.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t map_source_corresp_to_location(a_source_correspondence sc, gcc_jit_location **out_loc) {
    *out_loc = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    if (sc.source_line == 0 || !sc.source_file_name) {
        return GCC_GEN_BE_SUCCESS; /* No location */
    }
    
    int line = (int)sc.source_line;
    int col = (int)sc.source_column;
    if (col <= 0) col = 1;
    
    *out_loc = gcc_jit_context_new_location(ctx, sc.source_file_name, line, col);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the JIT location for a given IL type.
 *
 * @param tp The frontend type.
 * @param out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_type(a_type_ptr tp, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!tp) return GCC_GEN_BE_SUCCESS;
    return map_source_corresp_to_location(tp->source_corresp, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL variable.
 *
 * @param var The frontend variable.
 * @param out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_var(a_variable_ptr var, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!var) return GCC_GEN_BE_SUCCESS;
    return map_source_corresp_to_location(var->source_corresp, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL routine.
 *
 * @param rout The frontend routine.
 * @param out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_routine(a_routine_ptr rout, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!rout) return GCC_GEN_BE_SUCCESS;
    return map_source_corresp_to_location(rout->source_corresp, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL expression.
 *
 * @param expr The frontend expression.
 * @param out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(an_expr_node_ptr expr, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!expr) return GCC_GEN_BE_SUCCESS;
    return map_source_corresp_to_location(expr->source_corresp, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL statement.
 *
 * @param stmt The frontend statement.
 * @param out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!stmt) return GCC_GEN_BE_SUCCESS;
    return map_source_corresp_to_location(stmt->source_corresp, out_loc);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
