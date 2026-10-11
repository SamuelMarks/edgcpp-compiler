#include "basic_hdrs.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_builtin.h"
#include "const_ints.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

USING_NAMESPACE_EDG

namespace edg {
    int gcc_be_opt_level = 0;
    a_boolean gcc_be_debug_info = FALSE;
    a_boolean gcc_be_dump_initial_tree = FALSE;
    a_boolean gcc_be_dump_gimple = FALSE;
    a_boolean gcc_be_fPIC = FALSE;
    a_boolean gcc_be_fPIE = FALSE;
    a_boolean gcc_mode = FALSE;
    a_const_char *named_register_names[anr_last + 1] = { NULL };
    
    a_targ_size_t f_size_of_type(a_type_ptr t) { return 4; }
    a_host_large_integer value_of_integer_constant(a_constant *cp, a_boolean *err) { return 10; }
    double fetch_host_fp_value(a_float_kind fk, an_internal_float_value *fv) { return 0.0; }
    gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT { *out_lval = NULL; return GCC_GEN_BE_SUCCESS; }
    gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT { *out_func = NULL; return GCC_GEN_BE_SUCCESS; }
    gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT { *out_rval = NULL; return GCC_GEN_BE_SUCCESS; }

    gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
        *out_type = gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT);
        return GCC_GEN_BE_SUCCESS;
    }
}

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
}

using namespace edg;

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return 1;
    (void)gcc_gen_be_set_context(ctx);

    gcc_gen_be_error_t err;

    a_variable *var = (a_variable *)calloc(1, sizeof(a_variable));
    var->storage_class = sc_auto;

    a_statement *stmt = (a_statement *)calloc(1, sizeof(a_statement));
    stmt->kind = stmk_vla_decl;
    stmt->variant.vla.is_typedef_decl = FALSE;
    stmt->variant.vla.variant.variable = var;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(dummy_func, "entry");
    (void)gcc_gen_be_set_current_block(block);
    (void)gcc_gen_be_push_block(block);
    (void)gcc_gen_be_push_scope();

    err = gcc_gen_be_lower_statement(stmt, dummy_func);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    (void)gcc_gen_be_pop_scope();
    gcc_jit_block *popped = NULL;
    (void)gcc_gen_be_pop_block(&popped);

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
