#include "basic_hdrs.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include <libgccjit.h>
#include <stdio.h>

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
    
    gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
        *out_type = gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT);
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = gcc_jit_context_new_rvalue_from_int(NULL, gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT), 1);
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

    a_type *dummy_class = (a_type *)calloc(1, sizeof(a_type));
    dummy_class->kind = tk_class;
    a_class_type_supplement *ctsp = (a_class_type_supplement *)calloc(1, sizeof(a_class_type_supplement));
    ctsp->highest_virtual_function_number = 3;
    dummy_class->variant.class_struct_union.extra_info = ctsp;

    struct gcc_jit_lvalue *vtable = NULL;
    err = gcc_gen_be_emit_vtable(dummy_class, &vtable);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    struct gcc_jit_lvalue *vtt = NULL;
    err = gcc_gen_be_emit_vtt(dummy_class, &vtt);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    a_routine *dummy_thunk = (a_routine *)calloc(1, sizeof(a_routine));
    struct gcc_jit_function *thunk_fn = NULL;
    err = gcc_gen_be_emit_thunk(dummy_thunk, &thunk_fn);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    an_expr_node *dummy_expr = (an_expr_node *)calloc(1, sizeof(an_expr_node));
    dummy_expr->kind = enk_typeid;
    an_expr_node dummy_type_op;
    memset(&dummy_type_op, 0, sizeof(dummy_type_op));
    dummy_type_op.kind = enk_type_operand;
    dummy_type_op.variant.type_operand.type = dummy_class;
    dummy_expr->variant.typeid_info.type_with_opt_expr = &dummy_type_op;

    struct gcc_jit_rvalue *typeid_rval = NULL;
    err = gcc_gen_be_lower_typeid(dummy_expr, &typeid_rval);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    struct gcc_jit_lvalue *type_desc = NULL;
    err = gcc_gen_be_emit_type_descriptor(dummy_class, &type_desc);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
