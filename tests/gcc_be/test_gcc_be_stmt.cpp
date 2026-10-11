#include "basic_hdrs.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
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
    
    // Mocks for globals we don't have
    gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
        *out_type = gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT);
        return GCC_GEN_BE_SUCCESS;
    }
    
    long long value_of_integer_constant(a_constant* con, a_boolean* err) {
        return 42;
    }
    double fetch_host_fp_value(a_float_kind kind, an_internal_float_value* val) {
        return 3.14;
    }
    
    gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = gcc_jit_context_new_rvalue_from_int(NULL, gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT), 1);
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
        *out_lval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr tp, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = gcc_jit_context_new_rvalue_from_int(NULL, gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT), 0);
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = gcc_jit_context_new_rvalue_from_int(NULL, gcc_jit_context_get_type(NULL, GCC_JIT_TYPE_INT), 1);
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
        *out_loc = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
}

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
    
    // Mocks for lib_loader optionally loaded ptrs
    void *p_gcc_jit_block_end_with_extended_asm_goto = NULL;
    void *p_gcc_jit_block_add_extended_asm = NULL;
}

static a_statement* make_empty_stmt() {
    a_statement* stmt = (a_statement*)calloc(1, sizeof(a_statement));
    stmt->kind = stmk_empty;
    return stmt;
}

static a_statement* make_return_stmt() {
    a_statement* stmt = (a_statement*)calloc(1, sizeof(a_statement));
    stmt->kind = stmk_return;
    return stmt;
}

static a_statement* make_decl_stmt(a_variable_ptr var) {
    a_statement* stmt = (a_statement*)calloc(1, sizeof(a_statement));
    stmt->kind = stmk_decl;
    an_il_entity_list_entry_ptr ent = (an_il_entity_list_entry_ptr)calloc(1, sizeof(an_il_entity_list_entry));
    ent->entity.kind = iek_variable;
    ent->entity.ptr = (char*)var;
    stmt->variant.decl.entities = ent;
    return stmt;
}

static a_statement* make_init_stmt(a_variable_ptr var) {
    a_statement* stmt = (a_statement*)calloc(1, sizeof(a_statement));
    stmt->kind = stmk_init;
    a_dynamic_init_ptr dip = (a_dynamic_init_ptr)calloc(1, sizeof(a_dynamic_init));
    dip->kind = dik_zero;
    dip->variable = var;
    stmt->variant.dynamic_init = dip;
    return stmt;
}

using namespace edg;

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return 1;
    (void)gcc_gen_be_set_context(ctx);

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(dummy_func, "entry");
    (void)gcc_gen_be_set_current_block(block);
    (void)gcc_gen_be_push_block(block);

    gcc_gen_be_error_t err;

    // Test empty
    err = gcc_gen_be_lower_statement(make_empty_stmt(), dummy_func);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    // Test local variable declaration (stmk_decl)
    a_variable *local_var = (a_variable*)calloc(1, sizeof(a_variable));
    local_var->storage_class = sc_auto;
    local_var->is_parameter = FALSE;
    local_var->is_thread_local = FALSE;
    err = gcc_gen_be_lower_statement(make_decl_stmt(local_var), dummy_func);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    // Test static local variable initialization (stmk_init)
    a_variable *static_var = (a_variable*)calloc(1, sizeof(a_variable));
    static_var->storage_class = sc_static;
    static_var->is_parameter = FALSE;
    static_var->is_thread_local = FALSE;
    
    gcc_jit_lvalue *static_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "static_var");
    err = cache_insert(GCC_GEN_BE_CACHE_VAR, static_var, static_lval);
    if (err != GCC_GEN_BE_SUCCESS) return 1;
    
    err = gcc_gen_be_lower_statement(make_init_stmt(static_var), dummy_func);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    // Test return
    err = gcc_gen_be_lower_statement(make_return_stmt(), dummy_func);
    if (err != GCC_GEN_BE_SUCCESS) return 1;
    
    // Clean up blocks
    gcc_jit_block *popped = NULL;
    (void)gcc_gen_be_pop_block(&popped);

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
