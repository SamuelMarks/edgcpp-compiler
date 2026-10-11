#include "basic_hdrs.h"
#include "gcc_gen_be_context.h"
#include <stdio.h>

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
}

USING_NAMESPACE_EDG

int main() {
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = edg::gcc_gen_be_init();
    if (err != GCC_GEN_BE_SUCCESS) return 1;
    err = edg::gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return 1;
    (void)edg::gcc_gen_be_cleanup();
    printf("PASS\n");
    return 0;
}

namespace edg {
    int gcc_be_opt_level = 0;
    a_boolean gcc_be_debug_info = FALSE;
    a_boolean gcc_be_dump_initial_tree = FALSE;
    a_boolean gcc_be_dump_gimple = FALSE;
    a_boolean gcc_be_fPIC = FALSE;
    a_boolean gcc_be_fPIE = FALSE;
}
