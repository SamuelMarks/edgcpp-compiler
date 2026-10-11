#include "basic_hdrs.h"
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_context.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
}

int main() {
    printf("PASS\n");
    return 0;
}
