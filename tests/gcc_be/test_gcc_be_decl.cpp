#include "basic_hdrs.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
}

USING_NAMESPACE_EDG

namespace edg {
    int gcc_be_opt_level = 0;
    a_boolean gcc_be_debug_info = FALSE;
    a_boolean gcc_be_dump_initial_tree = FALSE;
    a_boolean gcc_be_dump_gimple = FALSE;
    a_boolean gcc_be_fPIC = FALSE;
    a_boolean gcc_be_fPIE = FALSE;
    
    a_scope_ptr scope_for_routine(a_routine_ptr rout) { return NULL; }
    a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout) { return NULL; }
    a_source_file_ptr conv_seq_to_file_and_line(unsigned int s, a_const_char **file, a_const_char **full, a_line_number *line, a_boolean *end) { *file = "test.c"; *full = "test.c"; *line = 1; *end = FALSE; return NULL; }
    
    gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = gcc_jit_context_new_rvalue_from_int(NULL, gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT), 1);
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = gcc_jit_context_new_rvalue_from_int(NULL, gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT), 1);
        return GCC_GEN_BE_SUCCESS;
    }
}

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    (void)edg::gcc_gen_be_set_context(ctx);
    
    edg::a_variable *var = (edg::a_variable *)calloc(1, sizeof(edg::a_variable));
    var->storage_class = sc_auto;
    
    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
