/**
 * @file test_llvm_exceptions.cpp
 * @brief Unit tests for the LLVM backend Exception Handling subsystem.
 * @details Validates function coverage of llvm_lower_try_block_stmt, llvm_lower_microsoft_try_stmt, and cleanups.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_eh.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  // Stubs
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `conv_seq_to_file_and_line`.
   * @param[out] arg_1 Parameter `arg_1` for `conv_seq_to_file_and_line`.
   * @param[out] arg_2 Parameter `arg_2` for `conv_seq_to_file_and_line`.
   * @param[in] arg_3 Parameter `arg_3` for `conv_seq_to_file_and_line`.
   * @param[in] arg_4 Parameter `arg_4` for `conv_seq_to_file_and_line`.
   * @return Result of `conv_seq_to_file_and_line` (a_source_file_ptr).
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number arg_0, a_const_char** arg_1, a_const_char** arg_2, a_line_number* arg_3, a_boolean* arg_4) { return nullptr; }
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  int g_eh_fail_type = 0; ///< When non-zero, `get_llvm_type` fails.
  int g_eh_fail_ti = 0; ///< Countdown of calls before `get_typeinfo_global` fails (negative = immediate fail).
  int g_eh_fail_stmt = 0; ///< Countdown of calls before `llvm_lower_statement` fails (negative = immediate fail).
  int g_eh_term_stmt = 0; ///< When non-zero, `llvm_lower_statement` emits `CreateUnreachable()` to terminate the block.
  int g_eh_push_cleanup_in_try = 0; ///< When non-zero, `llvm_lower_statement` pushes cleanups onto `cleanup_stack`.
  int g_eh_fail_expr = 0; ///< When non-zero, `llvm_lower_expression` fails.

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (g_eh_fail_type) return llvm_gen_be_error_t::unsupported_type;
    if (ty && ty->kind == tk_void) {
      *out_ty = llvm::Type::getVoidTy(*be_state->context);
    } else if (ty && ty->kind == tk_pointer) {
      *out_ty = llvm::PointerType::getUnqual(*be_state->context);
    } else {
      *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] type Parameter `type` for `get_typeinfo_global`.
   * @param[out] out_ti Parameter `out_ti` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    (void)type;
    if (g_eh_fail_ti < 0) return llvm_gen_be_error_t::unsupported_type;
    if (g_eh_fail_ti > 0) {
      --g_eh_fail_ti;
      if (g_eh_fail_ti == 0) return llvm_gen_be_error_t::unsupported_type;
    }
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    (void)stmt;
    if (g_eh_fail_stmt < 0) return llvm_gen_be_error_t::unsupported_stmt;
    if (g_eh_fail_stmt > 0) {
      --g_eh_fail_stmt;
      if (g_eh_fail_stmt == 0) return llvm_gen_be_error_t::unsupported_stmt;
    }
    if (g_eh_push_cleanup_in_try) {
      static a_routine s_dtor_named = {}, s_dtor_unnamed = {};
      s_dtor_named.source_corresp.name = (char*)"try_dtor";
      s_dtor_unnamed.source_corresp.name = nullptr;
      llvm::Value* ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
      ScopeCleanup c_inactive = {}; c_inactive.is_active = false;
      ScopeCleanup c_guard = {}; c_guard.is_active = true; c_guard.is_guard_abort = true; c_guard.guard_abort_ptr = ptr;
      ScopeCleanup c_dtor1 = {}; c_dtor1.is_active = true; c_dtor1.dtor_routine = &s_dtor_named; c_dtor1.object_ptr = ptr;
      ScopeCleanup c_dtor2 = {}; c_dtor2.is_active = true; c_dtor2.dtor_routine = &s_dtor_unnamed; c_dtor2.object_ptr = ptr;
      ScopeCleanup c_noop = {}; c_noop.is_active = true;
      be_state->cleanup_stack.cleanups.push_back(c_inactive);
      be_state->cleanup_stack.cleanups.push_back(c_guard);
      be_state->cleanup_stack.cleanups.push_back(c_dtor1);
      be_state->cleanup_stack.cleanups.push_back(c_dtor2);
      be_state->cleanup_stack.cleanups.push_back(c_noop);
    }
    if (g_eh_term_stmt) {
      be_state->builder->CreateUnreachable();
    }
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_exceptions.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    (void)expr;
    if (g_eh_fail_expr) return llvm_gen_be_error_t::unsupported_expr;
    if (out_val) *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
}

/**
 * @brief Entry point for the Itanium and MSVC SEH exception handling unit test suite.
 * @details Exercises `try`/`catch`, RAII scope cleanups, `__try`/`__except`/`__finally`, and exception specs.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_exceptions...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_eh", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // 1. Negative tests for try_block and cleanup
  assert(llvm_lower_try_block_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  a_statement wrong_stmt = {};
  wrong_stmt.kind = stmk_empty;
  assert(llvm_lower_try_block_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  a_routine r = {};
  llvm::Value* obj = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  assert(llvm_emit_eh_cleanup(nullptr, obj) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_eh_cleanup(&r, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_eh_cleanup(&r, obj) == llvm_gen_be_error_t::ok);

  // 2. Itanium ABI try-catch test
  a_statement stmt = {};
  stmt.kind = stmk_try_block;
  a_try_supplement try_block = {};
  stmt.variant.try_block = &try_block;
  
  a_handler handler = {};
  try_block.handlers = &handler;
  a_variable param = {};
  a_type ty = {};
  ty.kind = tk_integer;
  param.type = &ty;
  handler.parameter = &param;
  
  a_statement body = {};
  body.kind = stmk_empty;
  try_block.statement = &body;
  handler.statement = &body;

  assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.local_vars.count(&param) > 0 && state.local_vars[&param] != nullptr);

  // 3. MSVC ABI C++ try-catch test with active cleanup destructor
  state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  r.source_corresp.name = (char*)"??1MyObj@@QEAA@XZ";
  assert(llvm_emit_eh_cleanup(&r, obj) == llvm_gen_be_error_t::ok);
  bb = llvm::BasicBlock::Create(*state.context, "entry2", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("??1MyObj@@QEAA@XZ") != nullptr);

  // 4. MSVC SEH __try / __except test
  bb = llvm::BasicBlock::Create(*state.context, "entry3", f);
  state.builder->SetInsertPoint(bb);

  assert(llvm_lower_microsoft_try_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_microsoft_try_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  a_statement ms_stmt = {};
  ms_stmt.kind = stmk_microsoft_try;
  a_microsoft_try_supplement ms_sup = {};
  ms_stmt.variant.microsoft_try = &ms_sup;
  ms_sup.guarded_statement = &body;
  an_expr_node except_expr = {};
  except_expr.kind = enk_constant;
  ms_sup.except_expr = &except_expr;
  ms_sup.cleanup_statement = &body;

  assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::ok);

  // 5. MSVC SEH __try / __finally test
  bb = llvm::BasicBlock::Create(*state.context, "entry4", f);
  state.builder->SetInsertPoint(bb);

  a_statement ms_finally_stmt = {};
  ms_finally_stmt.kind = stmk_microsoft_try;
  a_microsoft_try_supplement ms_finally_sup = {};
  ms_finally_stmt.variant.microsoft_try = &ms_finally_sup;
  ms_finally_sup.guarded_statement = &body;
  ms_finally_sup.except_expr = nullptr;
  ms_finally_sup.cleanup_statement = &body;

  // 6. Intrinsic Exception Functions & SEH setjmp/longjmp
  bb = llvm::BasicBlock::Create(*state.context, "entry_intrinsics", f);
  state.builder->SetInsertPoint(bb);
  llvm::Value* out_val = nullptr;

  assert(llvm_lower_eh_exceptioncode(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_eh_exceptioncode(&out_val) == llvm_gen_be_error_t::ok);

  assert(llvm_lower_eh_exceptioninfo(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_eh_exceptioninfo(&out_val) == llvm_gen_be_error_t::ok);

  llvm::FunctionType* filter_ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*state.context), {llvm::PointerType::getUnqual(*state.context), llvm::PointerType::getUnqual(*state.context)}, false);
  llvm::Function* filter_func = llvm::Function::Create(filter_ft, llvm::Function::InternalLinkage, "filt2", state.module.get());
  llvm::BasicBlock* filter_entry = llvm::BasicBlock::Create(*state.context, "entry", filter_func);
  state.builder->SetInsertPoint(filter_entry);
  assert(llvm_lower_eh_exceptioninfo(&out_val) == llvm_gen_be_error_t::ok);

  llvm::Value* jmp_buf = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
  llvm::Value* longjmp_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1);
  
  assert(llvm_lower_seh_setjmp(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_setjmp(jmp_buf, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_setjmp(jmp_buf, &out_val) == llvm_gen_be_error_t::ok);

  assert(llvm_lower_seh_longjmp(nullptr, longjmp_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_longjmp(jmp_buf, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_longjmp(jmp_buf, longjmp_val) == llvm_gen_be_error_t::ok);

  // 7. Exception Specifications
  a_routine spec_rout = {};
  a_type spec_rout_ty = {};
  spec_rout_ty.kind = tk_routine;
  spec_rout.type = &spec_rout_ty;
  a_routine_type_supplement rts = {};
  spec_rout_ty.variant.routine.extra_info = &rts;

  an_exception_specification noexcept_spec = {};
  noexcept_spec.is_noexcept = TRUE;
  rts.exception_specification = &noexcept_spec;

  llvm::BasicBlock* lpad_bb = llvm::BasicBlock::Create(*state.context, "lpad", f);
  assert(llvm_emit_exception_specification_lpad(nullptr, lpad_bb) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_exception_specification_lpad(&spec_rout, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad_bb) == llvm_gen_be_error_t::ok);

  an_exception_specification throw_spec = {};
  throw_spec.is_noexcept = FALSE;
  an_exception_specification_type ex_ty = {};
  ex_ty.type = &ty;
  throw_spec.variant.exception_specification_type_list = &ex_ty;
  rts.exception_specification = &throw_spec;

  llvm::BasicBlock* lpad2_bb = llvm::BasicBlock::Create(*state.context, "lpad2", f);
  assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad2_bb) == llvm_gen_be_error_t::ok);

  // 8. Chained Cleanup Landing Pads (Itanium & MSVC)
  llvm::BasicBlock* cleanup_lpad_bb = nullptr;
  assert(llvm_emit_cleanup_landing_pad(0, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
  assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::ok);
  assert(cleanup_lpad_bb != nullptr);
  assert(llvm_emit_cleanup_landing_pad(0, lpad2_bb, &cleanup_lpad_bb) == llvm_gen_be_error_t::ok);
  assert(cleanup_lpad_bb != nullptr);

  state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::ok);
  assert(cleanup_lpad_bb != nullptr);

  // 9. Exhaustive branch coverage for llvm_gen_be_eh.cpp
  {
    // Microsoft SEH __try / __finally and __try / __except edge cases:
    // Clear escaped_var_indices then call ms_finally_stmt twice so escaped_var_indices is populated on 1st call and found on 2nd call!
    state.escaped_var_indices.clear();
    bb = llvm::BasicBlock::Create(*state.context, "entry_ms_fin1", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::ok);
    // Also call ms_stmt (__try / __except) a second time so escaped_var_indices is found in filter!
    assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::ok);

    // __try / __finally with cleanup_statement == nullptr (fin_func == nullptr), guarded_statement == nullptr, and active_funclet_pads non-empty
    ms_finally_sup.cleanup_statement = nullptr;
    ms_finally_sup.guarded_statement = nullptr;
    state.active_funclet_pads.push_back(llvm::ConstantTokenNone::get(*state.context));
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::ok);
    state.active_funclet_pads.clear();

    // __try / __finally with terminated guarded_statement
    ms_finally_sup.guarded_statement = &body;
    g_eh_term_stmt = 1;
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::ok);
    g_eh_term_stmt = 0;

    // __try / __finally errors: fail in cleanup_statement (call 1) and fail in guarded_statement (call 2)
    ms_finally_sup.cleanup_statement = &body;
    g_eh_fail_stmt = 1;
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 2;
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 0;

    // __try / __except errors: fail in guarded_statement, fail in except_expr, fail in cleanup_statement, and cleanup_statement == nullptr
    g_eh_fail_stmt = 1;
    assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 2;
    assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 0;
    g_eh_fail_expr = 1;
    assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_eh_fail_expr = 0;
    ms_sup.cleanup_statement = nullptr;
    assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::ok);

    // llvm_lower_eh_exceptioncode in a block starting with a CatchPadInst vs starting with an AllocaInst
    llvm::BasicBlock* cpad_disp_bb = llvm::BasicBlock::Create(*state.context, "cpad.disp", f);
    llvm::BasicBlock* cpad_bb = llvm::BasicBlock::Create(*state.context, "cpad.bb", f);
    state.builder->SetInsertPoint(cpad_disp_bb);
    llvm::CatchSwitchInst* cs = state.builder->CreateCatchSwitch(llvm::ConstantTokenNone::get(*state.context), nullptr, 1);
    cs->addHandler(cpad_bb);
    state.builder->SetInsertPoint(cpad_bb);
    state.builder->CreateCatchPad(cs, {});
    assert(llvm_lower_eh_exceptioncode(&out_val) == llvm_gen_be_error_t::ok);

    llvm::BasicBlock* non_cpad_bb = llvm::BasicBlock::Create(*state.context, "non.cpad.bb", f);
    state.builder->SetInsertPoint(non_cpad_bb);
    state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    assert(llvm_lower_eh_exceptioncode(&out_val) == llvm_gen_be_error_t::ok);

    // MSVC C++ try-catch edge cases:
    // named parameter, void parameter type, catch-all (parameter == nullptr), null try/handler statements,
    // terminated try block, active_funclet_pads non-empty, all cleanup kinds (inactive, guard_abort, finally_func, named/unnamed dtor, noop), and error paths
    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    a_type void_ty = {}, ptr_ty = {};
    void_ty.kind = tk_void;
    ptr_ty.kind = tk_pointer;
    a_variable named_void_param = {}, unnamed_int_param = {};
    named_void_param.source_corresp.name = (char*)"named_exc";
    named_void_param.type = &void_ty;
    unnamed_int_param.source_corresp.name = nullptr;
    unnamed_int_param.type = &ty;

    a_handler catch_all_h = {}, unnamed_int_h = {};
    catch_all_h.parameter = nullptr;
    catch_all_h.statement = nullptr;
    unnamed_int_h.parameter = &unnamed_int_param;
    unnamed_int_h.statement = &body;
    unnamed_int_h.next = &catch_all_h;
    handler.parameter = &named_void_param;
    handler.next = &unnamed_int_h;

    // Populate all cleanup kinds on MSVC
    llvm::Value* null_i8p = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*state.context));
    a_routine unnamed_dtor = {};
    unnamed_dtor.source_corresp.name = nullptr;
    llvm::FunctionType* fin_fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context),
        {llvm::Type::getInt8Ty(*state.context), llvm::PointerType::getUnqual(*state.context)}, false);
    llvm::Function* dummy_fin_fn = llvm::Function::Create(fin_fty, llvm::Function::InternalLinkage, "dummy_fin", state.module.get());

    ScopeCleanup sc_inact = {}; sc_inact.is_active = false;
    ScopeCleanup sc_guard = {}; sc_guard.is_active = true; sc_guard.is_guard_abort = true; sc_guard.guard_abort_ptr = null_i8p;
    ScopeCleanup sc_fin = {}; sc_fin.is_active = true; sc_fin.finally_func = dummy_fin_fn;
    ScopeCleanup sc_dtor_un = {}; sc_dtor_un.is_active = true; sc_dtor_un.dtor_routine = &unnamed_dtor; sc_dtor_un.object_ptr = null_i8p;
    ScopeCleanup sc_noop = {}; sc_noop.is_active = true;
    state.cleanup_stack.cleanups = {sc_inact, sc_guard, sc_fin, sc_dtor_un, sc_noop};

    state.active_funclet_pads.push_back(llvm::ConstantTokenNone::get(*state.context));
    try_block.statement = nullptr;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    state.active_funclet_pads.clear();

    // MSVC try block that pushes cleanups inside the try body (size > msvc_try_cleanup_depth)
    try_block.statement = &body;
    state.cleanup_stack.cleanups = {sc_noop};
    g_eh_push_cleanup_in_try = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_eh_push_cleanup_in_try = 0;
    state.cleanup_stack.cleanups.clear();

    // Second call hits named_void_param already in state.local_vars!
    try_block.statement = &body;
    g_eh_term_stmt = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_eh_term_stmt = 0;

    // Error paths in MSVC try-catch: try body error, get_typeinfo_global error, get_llvm_type error, handler statement error
    g_eh_fail_stmt = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 2;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 0;
    g_eh_fail_ti = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_eh_fail_ti = 0;
    state.local_vars.erase(&named_void_param);
    g_eh_fail_type = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_eh_fail_type = 0;

    // Itanium C++ try-catch edge cases:
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    a_variable ptr_param = {};
    ptr_param.source_corresp.name = (char*)"ptr_exc";
    ptr_param.type = &ptr_ty;
    handler.parameter = &ptr_param;
    g_eh_push_cleanup_in_try = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_eh_push_cleanup_in_try = 0;

    // Second call with ptr_param already in local_vars, void_ty param, null try statement, and terminated try/handler statements
    a_variable void_it_param = {};
    void_it_param.source_corresp.name = nullptr;
    void_it_param.type = &void_ty;
    a_handler h2_ptr = {};
    h2_ptr.parameter = &ptr_param;
    h2_ptr.statement = &body;
    h2_ptr.next = &catch_all_h;
    handler.parameter = &void_it_param;
    handler.next = &h2_ptr;
    try_block.statement = nullptr;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);

    try_block.statement = &body;
    g_eh_term_stmt = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_eh_term_stmt = 0;

    // Nested Itanium try block with outer landing pad in current_landing_pads
    llvm::BasicBlock* outer_lpad = llvm::BasicBlock::Create(*state.context, "outer.lpad", f);
    state.current_landing_pads.push_back(outer_lpad);
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.clear();

    // Catch parameter with dynamic_init (copy constructor named & unnamed, destructor named & unnamed, normal & terminated catch body)
    a_routine catch_copy_ctor = {}, catch_dtor = {};
    catch_copy_ctor.source_corresp.name = (char*)"_ZN3ExcC1ERKS_";
    catch_dtor.source_corresp.name = (char*)"_ZN3ExcD1Ev";
    a_dynamic_init catch_dip = {};
    catch_dip.kind = dik_constructor;
    catch_dip.variant.constructor.ptr = &catch_copy_ctor;
    catch_dip.destructor = &catch_dtor;
    handler.dynamic_init = &catch_dip;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // Unnamed copy ctor & unnamed dtor
    catch_copy_ctor.source_corresp.name = nullptr;
    catch_dtor.source_corresp.name = nullptr;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // Terminated handler body with destructor
    g_eh_term_stmt = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_eh_term_stmt = 0;
    // dynamic_init with kind == dik_constructor but constructor.ptr == nullptr, and destructor == nullptr
    catch_dip.variant.constructor.ptr = nullptr;
    catch_dip.destructor = nullptr;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // dynamic_init with kind != dik_constructor
    catch_dip.kind = dik_zero;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    handler.dynamic_init = nullptr;

    // Error paths in Itanium try-catch: try body error, get_typeinfo_global error on call 1 (clause) & call 2 (dispatch), get_llvm_type error, handler statement error
    handler.next = nullptr;
    g_eh_fail_stmt = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 2;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_stmt);
    g_eh_fail_stmt = 0;
    g_eh_fail_ti = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_eh_fail_ti = 2;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_eh_fail_ti = 0;
    state.local_vars.erase(&void_it_param);
    g_eh_fail_type = 1;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_eh_fail_type = 0;

    // Exception specification lpad: unparented block, null routine->type, non-routine type, null rts, null exception_specification, Itanium noexcept & throw(T) success & error, and MSVC target
    llvm::BasicBlock* unparented_lpad = llvm::BasicBlock::Create(*state.context, "unparented");
    assert(llvm_emit_exception_specification_lpad(&spec_rout, unparented_lpad) == llvm_gen_be_error_t::invalid_argument);
    delete unparented_lpad;

    llvm::BasicBlock* lpad_it_ok1 = llvm::BasicBlock::Create(*state.context, "lpad.it.ok1", f);
    assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad_it_ok1) == llvm_gen_be_error_t::ok); // Itanium throw(T) success!

    rts.exception_specification = &noexcept_spec;
    llvm::BasicBlock* lpad_it_noexcept = llvm::BasicBlock::Create(*state.context, "lpad.it.noexcept", f);
    assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad_it_noexcept) == llvm_gen_be_error_t::ok); // Itanium noexcept success!
    rts.exception_specification = &throw_spec;

    g_eh_fail_ti = 1;
    llvm::BasicBlock* lpad_err_bb = llvm::BasicBlock::Create(*state.context, "lpad.err", f);
    assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad_err_bb) == llvm_gen_be_error_t::unsupported_type);
    g_eh_fail_ti = 0;

    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    llvm::BasicBlock* lpad_ms_bb = llvm::BasicBlock::Create(*state.context, "lpad.ms", f);
    assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad_ms_bb) == llvm_gen_be_error_t::ok);

    // Chained cleanup landing pad: null be_state, null builder, null insert block, fresh function without personality (MSVC & Itanium), active_funclet_pads non-empty, and all cleanup variants
    state.cleanup_stack.cleanups = {sc_inact, sc_dtor_un, sc_noop, sc_guard, sc_fin};
    ScopeCleanup sc_dtor_named = {}; sc_dtor_named.is_active = true; sc_dtor_named.dtor_routine = &r; sc_dtor_named.object_ptr = null_i8p;
    state.cleanup_stack.cleanups.push_back(sc_dtor_named);

    llvm::Function* fresh_ms_fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "fresh_ms", state.module.get());
    llvm::BasicBlock* fresh_ms_bb = llvm::BasicBlock::Create(*state.context, "entry", fresh_ms_fn);
    state.builder->SetInsertPoint(fresh_ms_bb);
    state.active_funclet_pads.push_back(llvm::ConstantTokenNone::get(*state.context));
    assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::ok);
    state.active_funclet_pads.clear();

    // Constructor function-try-block implicit rethrow on MSVC and Itanium
    state.current_routine_is_ctor_or_dtor = true;
    handler.statement = nullptr;
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);

    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    llvm::Function* fresh_it_fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "fresh_it", state.module.get());
    llvm::BasicBlock* fresh_it_bb = llvm::BasicBlock::Create(*state.context, "entry", fresh_it_fn);
    state.builder->SetInsertPoint(fresh_it_bb);
    assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);
    state.current_routine_is_ctor_or_dtor = false;

    state.cleanup_stack.cleanups = {sc_inact, sc_dtor_un, sc_noop, sc_guard, sc_fin, sc_dtor_named};
    assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::ok);

    // llvm_get_or_create_current_unwind_dest & llvm_get_or_create_clang_call_terminate coverage
    llvm::BasicBlock* unwind_dest_bb = nullptr;
    assert(llvm_get_or_create_current_unwind_dest(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_get_or_create_current_unwind_dest(&unwind_dest_bb) == llvm_gen_be_error_t::ok);
    assert(unwind_dest_bb != nullptr);
    ScopeCleanup sc_vla = {}; sc_vla.is_active = true; sc_vla.saved_stack_ptr = null_i8p;
    state.cleanup_stack.cleanups = {sc_inact, sc_vla};
    state.landing_pad_cleanup_depths.push_back(0);
    state.current_landing_pads.push_back(cleanup_lpad_bb);
    assert(llvm_get_or_create_current_unwind_dest(&unwind_dest_bb) == llvm_gen_be_error_t::ok);
    state.landing_pad_cleanup_depths.clear();
    state.current_landing_pads.clear();
    state.cleanup_stack.cleanups.clear();
    assert(llvm_get_or_create_current_unwind_dest(&unwind_dest_bb) == llvm_gen_be_error_t::ok);
    assert(unwind_dest_bb == nullptr);

    llvm::Function* clang_term = nullptr;
    assert(llvm_get_or_create_clang_call_terminate(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_get_or_create_clang_call_terminate(&clang_term) == llvm_gen_be_error_t::ok);
    assert(clang_term != nullptr);
    // Call a second time when __clang_call_terminate already exists in module
    assert(llvm_get_or_create_clang_call_terminate(&clang_term) == llvm_gen_be_error_t::ok);

    state.builder->ClearInsertionPoint();
    assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::invalid_argument);
    auto saved_builder = std::move(state.builder);
    assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::invalid_argument);
    state.builder = std::move(saved_builder);
    edg::be_state = nullptr;
    assert(llvm_emit_cleanup_landing_pad(0, nullptr, &cleanup_lpad_bb) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_get_or_create_current_unwind_dest(&unwind_dest_bb) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_get_or_create_clang_call_terminate(&clang_term) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_seh_leave() == llvm_gen_be_error_t::invalid_argument);
    edg::be_state = &state;

    // Phase 4: Test llvm_lower_seh_leave and llvm_enable_async_seh_attributes
    assert(llvm_enable_async_seh_attributes(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_enable_async_seh_attributes(f) == llvm_gen_be_error_t::ok);
    assert(f->hasFnAttribute("seh-asynch"));

    assert(llvm_lower_seh_leave() == llvm_gen_be_error_t::invalid_argument); // empty seh_leave_blocks
    auto saved_bld2 = std::move(state.builder);
    assert(llvm_lower_seh_leave() == llvm_gen_be_error_t::invalid_argument); // null builder
    state.builder = std::move(saved_bld2);

    llvm::BasicBlock* leave_bb = llvm::BasicBlock::Create(*state.context, "leave.tgt", f);
    llvm::BasicBlock* leave_entry = llvm::BasicBlock::Create(*state.context, "leave.entry", f);
    state.builder->SetInsertPoint(leave_entry);
    state.seh_leave_blocks.push_back(leave_bb);
    assert(llvm_lower_seh_leave() == llvm_gen_be_error_t::ok);
    state.seh_leave_blocks.clear();

    state.enable_async_seh = true;
    llvm::BasicBlock* async_seh_bb = llvm::BasicBlock::Create(*state.context, "async.seh", f);
    state.builder->SetInsertPoint(async_seh_bb);
    assert(llvm_lower_microsoft_try_stmt(&ms_finally_stmt) == llvm_gen_be_error_t::ok);
    state.enable_async_seh = false;

    // Workstream 6: Test llvm_get_or_create_msvc_throw_info across all branches
    llvm::Constant* msvc_ti_out = nullptr;
    a_type ti_scalar_ty = {};
    ti_scalar_ty.kind = tk_integer;
    ti_scalar_ty.size = 0;
    ti_scalar_ty.source_corresp.name = nullptr;
    assert(llvm_get_or_create_msvc_throw_info(nullptr, nullptr, &msvc_ti_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_get_or_create_msvc_throw_info(&ti_scalar_ty, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    edg::be_state = nullptr;
    assert(llvm_get_or_create_msvc_throw_info(&ti_scalar_ty, nullptr, &msvc_ti_out) == llvm_gen_be_error_t::invalid_argument);
    edg::be_state = &state;

    // Scalar type with size == 0 and unnamed source_corresp.name, dtor_fn == nullptr
    assert(llvm_get_or_create_msvc_throw_info(&ti_scalar_ty, nullptr, &msvc_ti_out) == llvm_gen_be_error_t::ok);
    assert(msvc_ti_out != nullptr);
    // Second call hits cached _TI global
    assert(llvm_get_or_create_msvc_throw_info(&ti_scalar_ty, f, &msvc_ti_out) == llvm_gen_be_error_t::ok);

    // Class type with extra_info == nullptr
    a_type ti_cls_noextra = {};
    ti_cls_noextra.kind = tk_class;
    ti_cls_noextra.size = 8;
    ti_cls_noextra.source_corresp.name = (char*)"TiClsNoExtra";
    assert(llvm_get_or_create_msvc_throw_info(&ti_cls_noextra, f, &msvc_ti_out) == llvm_gen_be_error_t::ok);

    // Struct type with extra_info != nullptr, base_classes with named/unnamed/zero-sized/null-type bases
    a_type ti_base1 = {}, ti_base2 = {};
    ti_base1.kind = tk_struct; ti_base1.size = 4; ti_base1.source_corresp.name = (char*)"TiBase1";
    ti_base2.kind = tk_class; ti_base2.size = 0; ti_base2.source_corresp.name = nullptr;
    a_base_class ti_bc1 = {}, ti_bc2 = {}, ti_bc_null = {};
    ti_bc1.type = &ti_base1; ti_bc1.offset = 0; ti_bc1.next = &ti_bc2;
    ti_bc2.type = &ti_base2; ti_bc2.offset = 4; ti_bc2.next = &ti_bc_null;
    ti_bc_null.type = nullptr; ti_bc_null.next = nullptr;
    a_class_type_supplement ti_cls_supp = {};
    ti_cls_supp.base_classes = &ti_bc1;
    a_type ti_der_struct = {};
    ti_der_struct.kind = tk_struct;
    ti_der_struct.size = 16;
    ti_der_struct.source_corresp.name = (char*)"TiDerStruct";
    ti_der_struct.variant.class_struct_union.extra_info = &ti_cls_supp;
    assert(llvm_get_or_create_msvc_throw_info(&ti_der_struct, f, &msvc_ti_out) == llvm_gen_be_error_t::ok);
  }

  printf("PASS\n");
  return 0;
}
