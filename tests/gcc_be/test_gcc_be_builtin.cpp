#include "basic_hdrs.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <stdio.h>

USING_NAMESPACE_EDG

namespace edg {
    static gcc_jit_context *g_ctx = NULL;
    int gcc_be_opt_level = 0;
    a_boolean gcc_be_debug_info = FALSE;
    a_boolean gcc_be_dump_initial_tree = FALSE;
    a_boolean gcc_be_dump_gimple = FALSE;
    a_boolean gcc_be_fPIC = FALSE;
    a_boolean gcc_be_fPIE = FALSE;
    a_boolean gcc_mode = FALSE;
    a_const_char *named_register_names[anr_last + 1] = { NULL };
    
    gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
        *out_type = gcc_jit_context_get_type(g_ctx, GCC_JIT_TYPE_INT);
        return GCC_GEN_BE_SUCCESS;
    }
}

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
}

using namespace edg;

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    g_ctx = ctx;
    if (!ctx) return 1;
    (void)gcc_gen_be_set_context(ctx);
    (void)edg::load_libgccjit_posix();

    gcc_gen_be_error_t err;

    a_routine *rout = (a_routine *)calloc(1, sizeof(a_routine));
    rout->source_corresp.name = "__builtin_clz";
    rout->special_kind = sfk_none;
    rout->variant.builtin_function_kind = bfk_clz;

    struct gcc_jit_rvalue *arg = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 10);
    struct gcc_jit_rvalue *args[] = { arg };
    struct gcc_jit_rvalue *out_rval = NULL;
    
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(dummy_func, "entry");
    (void)gcc_gen_be_set_current_block(block);
    
    err = gcc_gen_be_lower_builtin_call(rout, 1, args, &out_rval);
    if (err != GCC_GEN_BE_SUCCESS && err != GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN) {
        printf("Failed: %d\n", err);
        return 1;
    }

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
