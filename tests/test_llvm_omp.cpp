/**
 * @file test_llvm_omp.cpp
 * @brief Unit tests for the LLVM backend OpenMP lowering subsystem.
 * @details Validates OpenMP parallel, worksharing, synchronization, tasking, reduction, and target offload lowering.
 */

#include "llvm_gen_be_omp.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

namespace edg {
  /**
   * @brief Test stub or helper for `get_mangled_function_name`.
   * @details Provides mock or helper behavior for `get_mangled_function_name` within `tests/test_llvm_omp.cpp`.
   * @param[in] routine Parameter `routine` for `get_mangled_function_name`.
   * @return Result of `get_mangled_function_name` (char*).
   */
  char* get_mangled_function_name(a_routine_ptr routine) { return nullptr; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  a_targ_size_t targ_sizeof_pointer = 8;
  
  int g_omp_fail_type = 0; ///< When non-zero, `get_llvm_type` fails.
  int g_omp_fail_expr = 0; ///< Countdown of calls before `llvm_lower_expression` fails.
  int g_omp_expr_i64 = 0; ///< When non-zero, `llvm_lower_expression` returns `i64` instead of `i32`.

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_omp.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      (void)ty;
      if (g_omp_fail_type) return llvm_gen_be_error_t::unsupported_type;
      *out_ty = llvm::Type::getInt32Ty(*be_state->context);
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_omp.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] arg_3 Parameter `arg_3` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** arg_3) noexcept {
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_omp.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `pop_lexical_block`.
   * @details Provides mock or helper behavior for `pop_lexical_block` within `tests/test_llvm_omp.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `pop_lexical_block`.
   * @return Result of `pop_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* arg_0) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `push_lexical_block`.
   * @details Provides mock or helper behavior for `push_lexical_block` within `tests/test_llvm_omp.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `push_lexical_block`.
   * @param[in] arg_1 Parameter `arg_1` for `push_lexical_block`.
   * @param[out] arg_2 Parameter `arg_2` for `push_lexical_block`.
   * @return Result of `push_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DILexicalBlock** arg_2) noexcept { return llvm_gen_be_error_t::ok; }
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_omp.cpp`.
   * @param[in] ty Parameter `ty` for `get_typeinfo_global`.
   * @param[out] out_const Parameter `out_const` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_asm_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_asm_stmt` within `tests/test_llvm_omp.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_asm_stmt`.
   * @return Result of `llvm_lower_asm_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  
  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_omp.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
      (void)expr;
      if (g_omp_fail_expr < 0) return llvm_gen_be_error_t::unsupported_expr;
      if (g_omp_fail_expr > 0) {
          --g_omp_fail_expr;
          if (g_omp_fail_expr == 0) return llvm_gen_be_error_t::unsupported_expr;
      }
      if (g_omp_expr_i64) {
          *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 0);
      } else {
          *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      }
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_stmt` within `tests/test_llvm_omp.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_stmt`.
   * @return Result of `llvm_lower_coroutine_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_return_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_return_stmt` within `tests/test_llvm_omp.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_return_stmt`.
   * @return Result of `llvm_lower_coroutine_return_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
}


LLVMBackendState* edg::be_state = nullptr;

/**
 * @brief Entry point for the OpenMP runtime lowering unit test suite.
 * @details Exercises all `lower_omp_*` runtime emission functions.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_omp...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_omp", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt = {};
  stmt.kind = stmk_empty;
  stmt.position.seq = 42;
  stmt.position.column = 7;

  a_variable local_var = {};
  local_var.source_corresp.name = (char*)"shared_x";
  llvm::AllocaInst* local_alloca = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "shared_x");
  state.local_vars[&local_var] = local_alloca;

  assert(lower_omp_parallel_region(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::ok);
  llvm::Function* microtask = state.module->getFunction(".omp_microtask.");
  assert(microtask != nullptr && microtask->arg_size() == 3);

  assert(lower_omp_for_worksharing(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_for_worksharing(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_critical(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_critical(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getGlobalVariable("gomp_critical_user_.var", true) != nullptr);

  assert(lower_omp_barrier(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_barrier(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_target_region(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_target_region(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("__omp_offloading_42_test_func") != nullptr);
  assert(state.module->getNamedGlobal(".omp_offloading.entry.__omp_offloading_42_test_func") != nullptr);

  assert(lower_omp_task(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_task(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction(".omp_task_entry.") != nullptr);

  assert(lower_omp_taskwait(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_taskwait(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_taskgroup(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_taskgroup(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_reduce(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_reduce(&stmt) == llvm_gen_be_error_t::ok);


  assert(lower_omp_sections(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_sections(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_section(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_section(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_single(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_single(&stmt) == llvm_gen_be_error_t::ok);

  // Test private and firstprivate clauses in parallel region + pragma dispatch via llvm_lower_statement
  a_type int_ty = {};
  int_ty.kind = tk_integer;
  a_variable priv_var = {};
  priv_var.source_corresp.name = (char*)"priv_v";
  priv_var.type = &int_ty;
  a_variable fpriv_var = {};
  fpriv_var.source_corresp.name = (char*)"fpriv_v";
  fpriv_var.type = &int_ty;
  state.local_vars[&priv_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "priv_v");
  state.local_vars[&fpriv_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "fpriv_v");

  stmt.has_associated_pragma = TRUE;
  state.omp_pragmas[&stmt] = "omp parallel private(priv_v) firstprivate(fpriv_v)";
  assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);

  // Test canonical stmk_for loop bound and induction variable extraction via pragma dispatch
  a_variable ind_var = {};
  ind_var.source_corresp.name = (char*)"i";
  ind_var.type = &int_ty;
  state.local_vars[&ind_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "i");

  an_expr_node lb_expr = {};
  lb_expr.kind = enk_constant;
  a_dynamic_init init_dip = {};
  init_dip.kind = dik_expression;
  init_dip.variable = &ind_var;
  init_dip.variant.expression = &lb_expr;
  a_statement init_stmt = {};
  init_stmt.kind = stmk_init;
  init_stmt.variant.dynamic_init = &init_dip;

  an_expr_node cmp_op1 = {};
  an_expr_node cmp_op2 = {};
  cmp_op1.next = &cmp_op2;
  an_expr_node cond_expr = {};
  cond_expr.kind = enk_operation;
  cond_expr.variant.operation.kind = eok_lt;
  cond_expr.variant.operation.operands = &cmp_op1;

  an_expr_node inc_expr = {};
  inc_expr.kind = enk_constant;

  a_statement body_stmt = {};
  body_stmt.kind = stmk_empty;

  a_for_loop for_extra = {};
  for_extra.initialization = &init_stmt;
  for_extra.increment = &inc_expr;

  a_statement for_stmt = {};
  for_stmt.kind = stmk_for;
  for_stmt.expr = &cond_expr;
  for_stmt.variant.for_loop.statement = &body_stmt;
  for_stmt.variant.for_loop.extra_info = &for_extra;
  for_stmt.has_associated_pragma = TRUE;
  state.omp_pragmas[&for_stmt] = "omp for";

  assert(llvm_lower_statement(&for_stmt) == llvm_gen_be_error_t::ok);

  // Test remaining directive strings through llvm_dispatch_omp_pragma_stmt
  const char* directives[] = {
      "omp critical", "omp barrier", "omp target", "omp taskwait",
      "omp taskgroup", "omp task", "omp reduction(+:shared_x)",
      "omp reduction(-:shared_x)", "omp reduction(*:shared_x)",
      "omp reduction(&:shared_x)", "omp reduction(&&:shared_x)",
      "omp reduction(|:shared_x)", "omp reduction(||:shared_x)",
      "omp reduction(^:shared_x)", "omp reduction(min:shared_x)",
      "omp reduction(max:shared_x)", "omp reduce",
      "omp for schedule(dynamic, 4)", "omp for schedule(guided, 2)",
      "omp for schedule(runtime)", "omp sections", "omp section",
      "omp single", "omp unknown_directive"
  };
  for (const char* dir : directives) {
      state.omp_pragmas[&stmt] = dir;
      stmt.has_associated_pragma = TRUE;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // Exhaustive branch coverage for llvm_gen_be_omp.cpp
  {
    bool handled = false;
    assert(llvm_dispatch_omp_pragma_stmt(nullptr, &handled) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, nullptr) == llvm_gen_be_error_t::invalid_argument);
    stmt.has_associated_pragma = FALSE;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
    stmt.has_associated_pragma = TRUE;
    state.omp_pragmas.erase(&stmt);
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok); // directive == nullptr

    // Attribute-based directive text extraction (null args, non-token kind, null token, valid token)
    an_attribute attr1 = {}, attr2 = {}, attr3 = {}, attr4 = {};
    an_attribute_arg aarg2 = {}, aarg3 = {}, aarg4 = {};
    attr1.arguments = nullptr; attr1.next = &attr2;
    aarg2.kind = static_cast<an_attribute_arg_kind>(99); attr2.arguments = &aarg2; attr2.next = &attr3;
    aarg3.kind = aak_token; aarg3.variant.token = nullptr; attr3.arguments = &aarg3; attr3.next = &attr4;
    aarg4.kind = aak_token; aarg4.variant.token = (char*)"omp barrier"; attr4.arguments = &aarg4; attr4.next = nullptr;
    stmt.attributes = &attr1;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
    stmt.attributes = nullptr;
    stmt.has_associated_pragma = FALSE;

    // Empty function name and empty module source file name in get_or_create_ident_t
    state.module->setSourceFileName("");
    llvm::Function* anon_f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "", state.module.get());
    llvm::BasicBlock* anon_bb = llvm::BasicBlock::Create(*state.context, "entry", anon_f);
    state.builder->SetInsertPoint(anon_bb);
    assert(lower_omp_barrier(&stmt) == llvm_gen_be_error_t::ok);
    state.builder->SetInsertPoint(bb);

    // Parallel region: null var/alloca in local_vars, unnamed var, untyped private var, unclosed clause, multiple clauses, get_llvm_type error, terminated body, and body error
    state.local_vars[nullptr] = local_alloca;
    a_variable null_alloca_var = {}, unnamed_var = {}, untyped_priv_var = {};
    state.local_vars[&null_alloca_var] = nullptr;
    unnamed_var.source_corresp.name = nullptr;
    state.local_vars[&unnamed_var] = local_alloca;
    untyped_priv_var.source_corresp.name = (char*)"untyped_p";
    untyped_priv_var.type = nullptr;
    state.local_vars[&untyped_priv_var] = local_alloca;

    state.omp_pragmas[&stmt] = "private(other) private(untyped_p) firstprivate(unclosed";
    assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::ok);

    // Body that terminates the block (stmk_return) inside parallel region, target region, and task
    a_statement ret_body = {};
    ret_body.kind = stmk_return;
    assert(lower_omp_parallel_region(&ret_body) == llvm_gen_be_error_t::ok);

    // Error when get_llvm_type fails on private var
    state.omp_pragmas[&stmt] = "private(priv_v)";
    g_omp_fail_type = 1;
    assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_omp_fail_type = 0;

    // Error when llvm_lower_statement fails (unsupported statement kind 99)
    a_statement bad_body = {};
    bad_body.kind = static_cast<a_statement_kind>(99);
    assert(lower_omp_parallel_region(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_for_worksharing(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_critical(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_target_region(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_task(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_taskgroup(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_reduce(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_sections(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_single(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);

    // Target region and task where kernel/task body terminates (stmk_return in current function too, so restoreIP to a fresh block afterwards)
    llvm::BasicBlock* tb1 = llvm::BasicBlock::Create(*state.context, "tb1", f);
    state.builder->SetInsertPoint(tb1);
    assert(lower_omp_target_region(&ret_body) == llvm_gen_be_error_t::ok);

    llvm::BasicBlock* tb2 = llvm::BasicBlock::Create(*state.context, "tb2", f);
    state.builder->SetInsertPoint(tb2);
    // For lower_omp_task, task_entry_fn returns i32, so use a block with an unreachable/br terminator
    a_label dummy_lbl = {};
    a_statement goto_body = {};
    goto_body.kind = stmk_goto;
    goto_body.variant.label.ptr = &dummy_lbl;
    assert(lower_omp_task(&goto_body) == llvm_gen_be_error_t::ok);

    llvm::BasicBlock* tb3 = llvm::BasicBlock::Create(*state.context, "tb3", f);
    state.builder->SetInsertPoint(tb3);

    // Worksharing for loop branches:
    // 1. fl->initialization == nullptr, stmt->expr == nullptr, fl->increment == nullptr
    for_stmt.has_associated_pragma = FALSE;
    for_extra.initialization = nullptr;
    for_stmt.expr = nullptr;
    for_extra.increment = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    // 3. stmk_init with dynamic_init == nullptr, dik != dik_expression, expression == nullptr, i64 lb_expr, and expr error
    for_extra.initialization = &init_stmt;
    init_stmt.variant.dynamic_init = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_stmt.variant.dynamic_init = &init_dip;
    init_dip.kind = dik_none;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_dip.kind = dik_expression;
    init_dip.variant.expression = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_dip.variant.expression = &lb_expr;
    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 4. stmk_expr assignment initialization (eok_assign, lhs enk_variable & non-variable, rhs present & absent, i64, error, non-assign)
    a_statement expr_init_s = {};
    expr_init_s.kind = stmk_expr;
    for_extra.initialization = &expr_init_s;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // expr == nullptr

    an_expr_node assign_expr = {}, lhs_node = {}, rhs_node = {};
    assign_expr.kind = enk_constant;
    expr_init_s.expr = &assign_expr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // kind != enk_operation

    assign_expr.kind = enk_operation;
    assign_expr.variant.operation.kind = eok_add;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // kind != eok_assign

    assign_expr.variant.operation.kind = eok_assign;
    assign_expr.variant.operation.operands = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // lhs == nullptr

    lhs_node.kind = enk_constant;
    assign_expr.variant.operation.operands = &lhs_node;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // lhs != enk_variable, rhs == nullptr

    a_variable unmapped_ind = {};
    lhs_node.kind = enk_variable;
    lhs_node.variant.variable.ptr = &unmapped_ind; // ind_var not in local_vars!
    lhs_node.next = &rhs_node;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 5. Condition expression: kind != enk_operation, op1 == nullptr, op2 == nullptr, eok_le, i64 ub_expr, error
    for_extra.initialization = nullptr;
    cond_expr.kind = enk_constant;
    for_stmt.expr = &cond_expr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    cond_expr.kind = enk_operation;
    cond_expr.variant.operation.operands = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    cmp_op1.next = nullptr;
    cond_expr.variant.operation.operands = &cmp_op1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    cmp_op1.next = &cmp_op2;
    cond_expr.variant.operation.kind = eok_le;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 6. Increment expression: i64 step_expr_val and error
    for_stmt.expr = nullptr;
    for_extra.increment = &inc_expr;
    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 7. Phase 10 directives: taskyield, flush, ordered, simd, teams distribute, target map(...), task depend(...)
    assert(lower_omp_taskyield(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_flush(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_ordered(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_simd(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_teams_distribute(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_taskloop(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_master(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_taskloop(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_master(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_taskloop(&goto_body) == llvm_gen_be_error_t::ok);
    assert(lower_omp_master(&stmt) == llvm_gen_be_error_t::ok);

    const char* p10_pragmas[] = {
        "omp taskyield",
        "omp flush",
        "omp ordered",
        "omp simd",
        "omp teams distribute",
        "omp taskloop",
        "omp master",
        "omp masked",
        "omp target update to(x)",
        "omp cancel parallel",
        "omp cancellation point parallel",
        "omp target map(to:x)",
        "omp target map(from:x)",
        "omp target map(alloc:x)",
        "omp task depend(in:x)",
        "omp task depend(out:x)",
        "omp task depend(inout:x)"
    };
    for (const char* p_str : p10_pragmas) {
      bool handled = false;
      stmt.has_associated_pragma = TRUE;
      state.omp_pragmas[&stmt] = p_str;
      assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
      assert(handled);
    }
    state.omp_pragmas.erase(&stmt);
    stmt.has_associated_pragma = FALSE;

    // Test collapse(2) and lastprivate(i) on lower_omp_for_worksharing
    a_statement inner_for = {};
    inner_for.kind = stmk_for;
    inner_for.variant.for_loop.extra_info = &for_extra;
    inner_for.variant.for_loop.statement = &body_stmt;

    for_stmt.variant.for_loop.statement = &inner_for;
    for_extra.initialization = &init_stmt;
    init_dip.variable = &ind_var;
    state.omp_pragmas[&for_stmt] = "omp for collapse(2) lastprivate(i)";
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    // Also test collapse(2) on non-for statement, for with null body, and for with non-for body
    state.omp_pragmas[&stmt] = "omp for collapse(2) lastprivate(i)";
    assert(lower_omp_for_worksharing(&stmt) == llvm_gen_be_error_t::ok);
    state.omp_pragmas.erase(&stmt);

    for_stmt.variant.for_loop.statement = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    for_stmt.variant.for_loop.statement = &body_stmt;
    // Also test lastprivate when ind_var is unmapped
    init_dip.variable = &unmapped_ind;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_dip.variable = &ind_var;
    state.omp_pragmas.erase(&for_stmt);

    // Error propagation in lower_omp_ordered, lower_omp_simd, lower_omp_teams_distribute (and terminated teams body)
    assert(lower_omp_ordered(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_simd(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_teams_distribute(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_teams_distribute(&ret_body) == llvm_gen_be_error_t::ok);

    // 8. Phase 19: EDG a_pragma_ptr AST node extraction in be_state->active_pragma_list and device offload bitcode
    a_pragma p_other = {}, p_null_txt = {}, p_match = {};
    p_other.entity.ptr = reinterpret_cast<char*>(&for_stmt);
    p_other.pragma_text = "omp barrier";
    p_null_txt.entity.ptr = reinterpret_cast<char*>(&stmt);
    p_null_txt.pragma_text = nullptr;
    p_match.entity.ptr = reinterpret_cast<char*>(&stmt);
    p_match.pragma_text = "omp barrier";
    p_other.next = &p_null_txt;
    p_null_txt.next = &p_match;
    state.active_pragma_list = &p_other;
    stmt.has_associated_pragma = TRUE;
    bool handled_ast = false;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled_ast) == llvm_gen_be_error_t::ok);
    assert(handled_ast);
    p_null_txt.next = nullptr;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled_ast) == llvm_gen_be_error_t::ok);
    assert(!handled_ast);
    stmt.has_associated_pragma = FALSE;
    state.active_pragma_list = nullptr;

    assert(llvm_emit_omp_device_offload_module(nullptr, "nvptx64-nvidia-cuda") == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_omp_device_offload_module(state.module.get(), nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_omp_device_offload_module(state.module.get(), "nvptx64-nvidia-cuda") == llvm_gen_be_error_t::ok);
    assert(llvm_emit_omp_device_offload_module(state.module.get(), "amdgcn-amd-amdhsa") == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("__llvm_offloading_device_bc") != nullptr);
    assert(state.module->getNamedGlobal(".omp_offloading.device_image") != nullptr);
    assert(state.module->getNamedGlobal(".omp_offloading.descriptor") != nullptr);
    assert(state.module->getFunction(".omp_offloading.descriptor_reg") != nullptr);
    assert(state.module->getFunction(".omp_offloading.descriptor_unreg") != nullptr);

    // Phase 2 OpenMP 5.x Device Offload & Fatbinary Bundling tests
    std::vector<omp_map_clause_entry_t> map_entries;
    assert(parse_omp_map_clauses(nullptr, &map_entries) == llvm_gen_be_error_t::invalid_argument);
    assert(parse_omp_map_clauses("map(to:x)", nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(parse_omp_map_clauses("map(unclosed", &map_entries) == llvm_gen_be_error_t::ok);
    assert(parse_omp_map_clauses(
               " to(shared_x) from(shared_x) map(tofrom: shared_x, , s.field) "
               "map(always, to: shared_x[2:16], shared_x[0:8], shared_x[a:b], shared_x[4], shared_x[4) "
               "map(shared_x[1:4]) map(from: unmapped_v) map(alloc: shared_x) "
               "map(delete: shared_x) map(shared_x) to(shared_x)",
               &map_entries) == llvm_gen_be_error_t::ok);
    assert(!map_entries.empty());

    assert(lower_omp_target_data(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_target_enter_data(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_target_exit_data(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_target_update(nullptr) == llvm_gen_be_error_t::invalid_argument);

    assert(lower_omp_target_data(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_target_data(&stmt) == llvm_gen_be_error_t::ok);
    assert(lower_omp_target_enter_data(&stmt) == llvm_gen_be_error_t::ok);
    assert(lower_omp_target_exit_data(&stmt) == llvm_gen_be_error_t::ok);
    assert(lower_omp_target_update(&stmt) == llvm_gen_be_error_t::ok);

    const char* p2_pragmas[] = {
        "omp target data map(tofrom: shared_x[2:16], s.field)",
        "omp target enter data map(to: shared_x)",
        "omp target exit data map(release: shared_x)",
        "omp target update from(shared_x)",
        "omp target nowait depend(in: shared_x) map(always, tofrom: shared_x[2:8])"
    };
    for (const char* p_str : p2_pragmas) {
      bool handled = false;
      stmt.has_associated_pragma = TRUE;
      state.omp_pragmas[&stmt] = p_str;
      assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
      assert(handled);
    }
    state.omp_pragmas.erase(&stmt);
    stmt.has_associated_pragma = FALSE;

    // Test compile_omp_device_module with declare target function and empty/non-empty arch
    llvm::Function* decl_tgt_fn = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "dev_helper_fn", state.module.get());
    decl_tgt_fn->addFnAttr("omp_declare_target");
    llvm::SmallVector<char, 512> dev_img;
    assert(compile_omp_device_module(nullptr, "nvptx64-nvidia-cuda", "sm_80", &dev_img) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(compile_omp_device_module(state.module.get(), "nvptx64-nvidia-cuda", "sm_80", nullptr) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(compile_omp_device_module(state.module.get(), "nvptx64-nvidia-cuda", "", &dev_img) ==
           llvm_gen_be_error_t::ok);
    assert(!dev_img.empty());
    assert(compile_omp_device_module(state.module.get(), "amdgcn-amd-amdhsa", "gfx90a", &dev_img) ==
           llvm_gen_be_error_t::ok);
    assert(!dev_img.empty());

    // Test lower_omp_declare_simd, lower_omp_declare_reduction, and lower_omp_atomic (read, write, update, capture, compare) + omp for simd
    assert(lower_omp_declare_simd(nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_simd(f, 0) == llvm_gen_be_error_t::ok);
    assert(lower_omp_declare_simd(f, 8) == llvm_gen_be_error_t::ok);
    assert(f->hasFnAttribute("vector-function-abi-variant"));

    llvm::Function* udr_comb_fn = nullptr;
    llvm::Type* i32_llvm_ty = llvm::Type::getInt32Ty(*state.context);
    llvm::Type* f32_llvm_ty = llvm::Type::getFloatTy(*state.context);
    assert(lower_omp_declare_reduction(nullptr, i32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_reduction("my_sum", nullptr, &udr_comb_fn) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_reduction("my_sum", i32_llvm_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_reduction("my_sum", i32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::ok);
    assert(udr_comb_fn != nullptr);
    assert(state.module->getFunction(".omp.user_reduction.init.my_sum") != nullptr);
    // Second call with same id reuses existing combiner and init functions
    assert(lower_omp_declare_reduction("my_sum", i32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::ok);
    // Floating-point reduction type exercises FP combiner branch
    assert(lower_omp_declare_reduction("my_fsum", f32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::ok);

    assert(lower_omp_atomic(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_atomic(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_atomic(&stmt) == llvm_gen_be_error_t::ok);

    const char* atomic_and_simd_pragmas[] = {
        "omp atomic read",
        "omp atomic write",
        "omp atomic update",
        "omp atomic capture",
        "omp atomic compare",
        "omp for simd"
    };
    for (const char* p_str : atomic_and_simd_pragmas) {
      bool handled = false;
      stmt.has_associated_pragma = TRUE;
      state.omp_pragmas[&stmt] = p_str;
      assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
      assert(handled);
    }
    bad_body.has_associated_pragma = TRUE;
    state.omp_pragmas[&bad_body] = "omp for simd";
    bool bad_handled = false;
    assert(llvm_dispatch_omp_pragma_stmt(&bad_body, &bad_handled) == llvm_gen_be_error_t::unsupported_stmt);
    state.omp_pragmas.erase(&bad_body);
    bad_body.has_associated_pragma = FALSE;
  }

  printf("PASS\n");
  return 0;
}
