#include "basic_hdrs.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>

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

    a_routine *rout = (a_routine *)calloc(1, sizeof(a_routine));
    an_attribute *attr = (an_attribute *)calloc(1, sizeof(an_attribute));
    attr->kind = ak_always_inline;
    rout->source_corresp.attributes = attr;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    
    err = gcc_gen_be_apply_function_attributes(rout, dummy_func);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
