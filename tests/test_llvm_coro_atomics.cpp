/**
 * @file test_llvm_coro_atomics.cpp
 * @brief Unit tests for the LLVM backend coroutine and atomic lowering subsystem.
 * @details Validates function coverage of llvm_lower_coroutine_stmt, llvm_lower_coroutine_return_stmt, and co_await/co_yield expressions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_coro.h"
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
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_coro_atomics.cpp`.
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
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_coro_atomics.cpp`.
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
  
  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] type Parameter `type` for `get_typeinfo_global`.
   * @param[out] out_ti Parameter `out_ti` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  int g_coro_expr_fail = 0; ///< When non-zero, `llvm_lower_expression` returns `unsupported_expr`.
  int g_coro_expr_null = 0; ///< When non-zero, `llvm_lower_expression` sets `*out_val = nullptr`.

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (g_coro_expr_fail < 0) return llvm_gen_be_error_t::unsupported_expr;
    if (g_coro_expr_fail > 0) {
      --g_coro_expr_fail;
      if (g_coro_expr_fail == 0) return llvm_gen_be_error_t::unsupported_expr;
    }
    if (expr == nullptr || g_coro_expr_null) {
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    if (expr->type) {
        if (expr->type->kind == tk_pointer) {
            *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
            return llvm_gen_be_error_t::ok;
        } else if (expr->type->kind == tk_integer && expr->type->variant.integer.bool_type) {
            *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), 1);
            return llvm_gen_be_error_t::ok;
        }
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
}

/**
 * @brief Entry point for the C++20 coroutine and atomic operations unit test suite.
 * @details Exercises coroutine frame setup, `co_await`, `co_yield`, `co_return`, and atomic intrinsics.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_coro_atomics...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_coro", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // 1. Negative tests
  assert(llvm_lower_coroutine_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  a_statement wrong_stmt = {};
  wrong_stmt.kind = stmk_empty;
  assert(llvm_lower_coroutine_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coroutine_return_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coroutine_return_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  llvm::Value* coro_val = nullptr;
  assert(llvm_lower_coro_suspend(nullptr, &coro_val) == llvm_gen_be_error_t::invalid_argument);
  an_expr_node wrong_expr = {};
  wrong_expr.kind = enk_constant;
  assert(llvm_lower_coro_suspend(&wrong_expr, &coro_val) == llvm_gen_be_error_t::invalid_argument);

  // 2. Coroutine setup statement
  a_statement stmt = {};
  stmt.kind = stmk_coroutine;
  a_coroutine_descr descr = {};
  stmt.variant.coroutine.descr = &descr;
  
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Custom allocator
  a_routine new_rt = {};
  new_rt.source_corresp.name = "custom_new";
  descr.new_routine = &new_rt;
  
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*state.context);
  llvm::FunctionType* custom_new_fty = llvm::FunctionType::get(int8_ptr_ty, {llvm::Type::getInt64Ty(*state.context), int32_ty}, false);
  llvm::Function::Create(custom_new_fty, llvm::Function::ExternalLinkage, "custom_new", state.module.get());
  
  // Create an argument in the parent function
  llvm::FunctionType* f_with_args = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int32_ty}, false);
  llvm::Function* parent_f = llvm::Function::Create(f_with_args, llvm::Function::ExternalLinkage, "coro_func", state.module.get());
  llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(*state.context, "entry", parent_f);
  state.builder->SetInsertPoint(bb2);
  
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  an_expr_node unhandled_ex = {};
  unhandled_ex.kind = enk_constant;
  descr.unhandled_exception_call = &unhandled_ex;
  a_label fin_lbl = {};
  descr.final_suspend_label = &fin_lbl;
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // 3. Coroutine return statement
  bb = llvm::BasicBlock::Create(*state.context, "entry_ret", f);
  state.builder->SetInsertPoint(bb);
  stmt.kind = stmk_coroutine_return;
  assert(llvm_lower_coroutine_return_stmt(&stmt) == llvm_gen_be_error_t::ok);
  
  // 4. Coroutine co_await expression
  bb = llvm::BasicBlock::Create(*state.context, "entry_await", f);
  state.builder->SetInsertPoint(bb);

  an_expr_node ready_op = {};
  ready_op.kind = enk_constant;
  an_expr_node resume_op = {};
  resume_op.kind = enk_constant;
  an_expr_node suspend_op = {};
  suspend_op.kind = enk_constant;

  ready_op.next = &resume_op;
  resume_op.next = &suspend_op;

  an_expr_node await_expr = {};
  await_expr.kind = enk_await;
  await_expr.variant.await_info.ready_resume_suspend = &ready_op;
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);
  assert(coro_val != nullptr);

  // Symmetric transfer await_suspend (pointer)
  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  an_expr_node suspend_ptr_op = {};
  suspend_ptr_op.kind = enk_constant;
  suspend_ptr_op.type = &ptr_ty;
  resume_op.next = &suspend_ptr_op;
  bb = llvm::BasicBlock::Create(*state.context, "entry_await_ptr", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);

  // Bool await_suspend (boolean)
  a_type bool_ty = {};
  bool_ty.kind = tk_integer;
  bool_ty.variant.integer.bool_type = TRUE;
  an_expr_node suspend_bool_op = {};
  suspend_bool_op.kind = enk_constant;
  suspend_bool_op.type = &bool_ty;
  resume_op.next = &suspend_bool_op;
  bb = llvm::BasicBlock::Create(*state.context, "entry_await_bool", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);

  // 5. Coroutine co_yield expression
  bb = llvm::BasicBlock::Create(*state.context, "entry_yield", f);
  state.builder->SetInsertPoint(bb);

  an_expr_node yield_expr = {};
  yield_expr.kind = enk_yield;
  yield_expr.variant.await_info.ready_resume_suspend = &ready_op;
  assert(llvm_lower_coro_suspend(&yield_expr, &coro_val) == llvm_gen_be_error_t::ok);
  assert(coro_val != nullptr);

  // Test target-specific global operator new fallback (MSVC 64-bit, MSVC 32-bit, Itanium 32-bit)
  descr.new_routine = nullptr;
  state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  bb = llvm::BasicBlock::Create(*state.context, "entry_msvc64", f);
  state.builder->SetInsertPoint(bb);
  stmt.kind = stmk_coroutine;
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("??2@YAPEAX_K@Z") != nullptr);

  state.module->setTargetTriple(llvm::Triple("i686-pc-windows-msvc"));
  bb = llvm::BasicBlock::Create(*state.context, "entry_msvc32", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("??2@YAPAXI@Z") != nullptr);

  state.module->setTargetTriple(llvm::Triple("i686-pc-linux-gnu"));
  bb = llvm::BasicBlock::Create(*state.context, "entry_itanium32", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("_Znwj") != nullptr);

  assert(llvm_lower_coro_halo_hints(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coro_halo_hints(f) == llvm_gen_be_error_t::ok);

  // 6. Exhaustive branch coverage for llvm_gen_be_coro.cpp
  {
    assert(llvm_lower_coro_suspend(&await_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_statement null_descr_stmt = {};
    null_descr_stmt.kind = stmk_coroutine;
    null_descr_stmt.variant.coroutine.descr = nullptr;
    assert(llvm_lower_coroutine_stmt(&null_descr_stmt) == llvm_gen_be_error_t::invalid_argument);

    // Promise in local_vars and not in local_vars (with type == nullptr and type != nullptr)
    a_variable prom_var = {}, unmapped_prom = {}, typed_prom = {};
    typed_prom.type = &bool_ty;
    state.local_vars[&prom_var] = state.builder->CreateAlloca(int32_ty, nullptr, "prom");
    descr.promise = &unmapped_prom;
    bb = llvm::BasicBlock::Create(*state.context, "entry_prom1", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    descr.promise = &typed_prom;
    bb = llvm::BasicBlock::Create(*state.context, "entry_prom_typed", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    // Test this_param_copy and parameter_copies (both when param_idx < arg_size and >= arg_size)
    a_variable this_copy = {}, pcopy1 = {}, pcopy2 = {};
    pcopy1.next = &pcopy2;
    descr.this_param_copy = &this_copy;
    descr.parameter_copies = &pcopy1;
    // First in 0-arg function `f` (param_idx >= func->arg_size())
    bb = llvm::BasicBlock::Create(*state.context, "entry_pcopy_0arg", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // Next in 1-arg function `parent_f` (this_param_copy < arg_size, pcopy1 >= arg_size)
    bb2 = llvm::BasicBlock::Create(*state.context, "entry_pcopy_1arg", parent_f);
    state.builder->SetInsertPoint(bb2);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.this_param_copy = nullptr;
    // Now in 1-arg function `parent_f` with this_param_copy == nullptr (pcopy1 < arg_size, pcopy2 >= arg_size)
    bb2 = llvm::BasicBlock::Create(*state.context, "entry_pcopy_1arg_nothis", parent_f);
    state.builder->SetInsertPoint(bb2);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.parameter_copies = nullptr;

    // Test get_return_object_call, initial_suspend_call, and final_suspend_call (ok and error paths, label cached vs uncached)
    an_expr_node gro_call = {}, init_susp_call = {}, fin_susp_call = {};
    gro_call.kind = enk_constant;
    init_susp_call.kind = enk_constant;
    fin_susp_call.kind = enk_constant;
    descr.get_return_object_call = &gro_call;
    descr.initial_suspend_call = &init_susp_call;
    descr.final_suspend_call = &fin_susp_call;
    state.label_blocks.clear(); // uncached final_suspend_label
    bb = llvm::BasicBlock::Create(*state.context, "entry_lifecycle_ok1", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // Second call hits cached final_suspend_label
    bb = llvm::BasicBlock::Create(*state.context, "entry_lifecycle_ok2", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // final_suspend_call != nullptr && final_suspend_label == nullptr
    descr.final_suspend_label = nullptr;
    bb = llvm::BasicBlock::Create(*state.context, "entry_lifecycle_nolbl", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.final_suspend_label = &fin_lbl;

    // Error paths on get_return_object_call (1), initial_suspend_call (2), final_suspend_call (3)
    g_coro_expr_fail = 1;
    bb = llvm::BasicBlock::Create(*state.context, "entry_gro_call_err", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 2;
    bb = llvm::BasicBlock::Create(*state.context, "entry_init_susp_err", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 3;
    bb = llvm::BasicBlock::Create(*state.context, "entry_fin_susp_err", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;
    descr.get_return_object_call = nullptr;
    descr.initial_suspend_call = nullptr;
    descr.final_suspend_call = nullptr;

    descr.promise = &prom_var;
    // Custom new_routine with null name, non-existent function, and multi-arg casting (IntCast, BitOrPointerCast, default null arg)
    a_routine new_null_name = {};
    new_null_name.source_corresp.name = nullptr;
    descr.new_routine = &new_null_name;
    bb = llvm::BasicBlock::Create(*state.context, "entry_new_null", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*state.context);
    llvm::Type* f32_ty = llvm::Type::getFloatTy(*state.context);
    llvm::FunctionType* cast_new_fty = llvm::FunctionType::get(int8_ptr_ty, {i64_ty, i64_ty, i64_ty, f32_ty, int32_ty}, false);
    llvm::Function::Create(cast_new_fty, llvm::Function::ExternalLinkage, "custom_new_cast", state.module.get());
    new_null_name.source_corresp.name = (char*)"custom_new_cast";

    // Non-void return coroutine function with 3 args (float, int32, int32) -> casts arg0 from float to i64, arg1 from i32 to i64 (IntCast), arg2 from i32 to float, arg3 defaults to 0!
    llvm::FunctionType* nonvoid_coro_fty = llvm::FunctionType::get(int8_ptr_ty, {f32_ty, int32_ty, int32_ty}, false);
    llvm::Function* nonvoid_f = llvm::Function::Create(nonvoid_coro_fty, llvm::Function::ExternalLinkage, "nonvoid_coro", state.module.get());
    llvm::BasicBlock* nv_bb = llvm::BasicBlock::Create(*state.context, "entry", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);

    // Custom delete_routine (1 arg, 2 args, 0 args, non-existent, and null name)
    a_routine del_rt = {};
    del_rt.source_corresp.name = (char*)"custom_del_2";
    llvm::FunctionType* del2_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty, i64_ty}, false);
    llvm::Function::Create(del2_fty, llvm::Function::ExternalLinkage, "custom_del_2", state.module.get());
    descr.delete_routine = &del_rt;

    // alloc_failure_gro_call returning i32 in ptr-returning function (casts fail_val), returning i32 in void-returning function (f), returning ptr (exact match), returning null, and failing
    an_expr_node gro_expr = {};
    gro_expr.kind = enk_constant;
    descr.alloc_failure_gro_call = &gro_expr;
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    // Also test alloc_failure_gro_call in void-returning function f (and unhandled_exception_call != nullptr with final_suspend_label == nullptr)
    descr.final_suspend_label = nullptr;
    bb = llvm::BasicBlock::Create(*state.context, "entry_gro_void", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.final_suspend_label = &fin_lbl;

    gro_expr.type = &ptr_ty; // exact match with nonvoid_f return type (int8_ptr_ty)
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_ptr", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    g_coro_expr_null = 1; // fail_val == nullptr
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_null", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 0;

    g_coro_expr_fail = 1; // alloc_failure_gro_call expression error
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_err", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;
    descr.alloc_failure_gro_call = nullptr;

    // Custom delete_routine with 1 arg, 0 args, non-existent, and null name
    llvm::FunctionType* del1_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty}, false);
    llvm::Function::Create(del1_fty, llvm::Function::ExternalLinkage, "custom_del_1", state.module.get());
    del_rt.source_corresp.name = (char*)"custom_del_1";
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del1", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    llvm::FunctionType* del0_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {}, false);
    llvm::Function::Create(del0_fty, llvm::Function::ExternalLinkage, "custom_del_0", state.module.get());
    del_rt.source_corresp.name = (char*)"custom_del_0";
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del0", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    del_rt.source_corresp.name = (char*)"nonexistent_del";
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del_no", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    del_rt.source_corresp.name = nullptr;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del_null", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.delete_routine = nullptr;

    // Unhandled exception error path on both Itanium and MSVC, and final_suspend_label not yet in label_blocks on MSVC
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    g_coro_expr_fail = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_uh_err_it", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);

    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    g_coro_expr_fail = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_uh_err_ms", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;

    state.label_blocks.clear(); // force it == label_blocks.end() on MSVC
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_uh_ok_ms", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    descr.unhandled_exception_call = nullptr;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_no_uh", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    descr.final_suspend_label = nullptr;

    // Coroutine return statement: with stmt->expr (ok & error), non-void function, and active_hdl in same function vs nullptr
    a_statement ret_stmt = {};
    ret_stmt.kind = stmk_coroutine_return;
    ret_stmt.expr = &gro_expr;
    assert(llvm_lower_coroutine_return_stmt(&ret_stmt) == llvm_gen_be_error_t::ok); // active_hdl in nonvoid_f!

    g_coro_expr_fail = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_ret_err", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_return_stmt(&ret_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;

    state.current_coro_handle = nullptr;
    assert(llvm_lower_coroutine_return_stmt(&ret_stmt) == llvm_gen_be_error_t::ok);

    // Coroutine suspend:
    // 1. ready_expr == nullptr, resume_expr == nullptr, suspend_expr == nullptr, in nonvoid_f with current_coro_cleanup_bb in nonvoid_f
    an_expr_node bare_await = {};
    bare_await.kind = enk_await;
    bare_await.variant.await_info.ready_resume_suspend = nullptr;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_bare_await", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

    // 2. ready_expr is bool (i1), ready_expr returns nullptr, ready_expr fails
    ready_op.type = &bool_ty;
    ready_op.next = nullptr; // resume_expr == nullptr, suspend_expr == nullptr
    bare_await.variant.await_info.ready_resume_suspend = &ready_op;
    state.current_coro_cleanup_bb = nullptr;
    state.current_coro_handle = state.builder->CreateAlloca(int32_ty); // instruction in nonvoid_f!
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

    // Also test when current_coro_handle is in a DIFFERENT function (f) while lowering in nonvoid_f
    llvm::BasicBlock* f_tmp_bb = llvm::BasicBlock::Create(*state.context, "tmp_hdl", f);
    state.builder->SetInsertPoint(f_tmp_bb);
    llvm::Value* hdl_in_f = state.builder->CreateAlloca(int32_ty);
    state.builder->SetInsertPoint(nv_bb);
    state.current_coro_handle = hdl_in_f;

    g_coro_expr_null = 1; // is_ready == nullptr, susp_val == nullptr, res_val == nullptr
    ready_op.next = &resume_op;
    resume_op.next = &suspend_op;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 0;

    // Error paths on ready_expr (call 1), suspend_expr (call 2), and resume_expr (call 3), plus current_coro_handle == nullptr
    state.current_coro_handle = nullptr;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
    g_coro_expr_fail = 1;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 2;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 3;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;

    // Test get_return_object_call in nonvoid_f (gro_val cast to return type, exact match, and gro_val == nullptr) + suspend loading current_coro_gro_slot (in same function vs different function) + final_suspend_bb set
    descr.get_return_object_call = &gro_call;
    gro_call.type = nullptr; // returns i32, cast to int8_ptr_ty
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_slot_cast", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    assert(state.current_coro_gro_slot != nullptr);
    state.current_coro_suspend_bb = nv_bb;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
    state.current_coro_suspend_bb = nullptr;

    // Also test current_coro_gro_slot belonging to a different function (f) when suspending in nonvoid_f
    llvm::BasicBlock* f_gro_bb = llvm::BasicBlock::Create(*state.context, "tmp_gro", f);
    state.builder->SetInsertPoint(f_gro_bb);
    llvm::Value* gro_in_f = state.builder->CreateAlloca(int8_ptr_ty);
    state.builder->SetInsertPoint(nv_bb);
    state.current_coro_gro_slot = gro_in_f;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

    // gro_call exact type match in nonvoid_f, and gro_call returning nullptr
    gro_call.type = &ptr_ty;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_slot_exact", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_slot_null", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 0;
    descr.get_return_object_call = nullptr;

    // Over-aligned coroutine promise (alignment = 32 > 16) on Itanium and MSVC
    a_variable typed_prom32 = {};
    bool_ty.alignment = 32;
    typed_prom32.type = &bool_ty;
    descr.promise = &typed_prom32;
    descr.new_routine = nullptr;
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_overaligned_it", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_overaligned_ms", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    bool_ty.alignment = 1;

    // Custom 3-arg aligned delete_routine(void*, size_t, align_val_t)
    llvm::FunctionType* del3_fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context), {int8_ptr_ty, i64_ty, i64_ty}, false);
    llvm::Function::Create(del3_fty, llvm::Function::ExternalLinkage, "custom_del_3", state.module.get());
    del_rt.source_corresp.name = (char*)"custom_del_3";
    descr.delete_routine = &del_rt;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del3", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.delete_routine = nullptr;

    // Direct validation of helper error branches and llvm_lower_coro_noop
    uint32_t copied_cnt = 0;
    assert(llvm_emit_coro_parameter_copies(nullptr, nonvoid_f, &copied_cnt) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_parameter_copies(&descr, nullptr, &copied_cnt) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_parameter_copies(&descr, nonvoid_f, nullptr) == llvm_gen_be_error_t::invalid_argument);

    llvm::Value* alloc_out = nullptr;
    llvm::Value* dummy_sz = llvm::ConstantInt::get(i64_ty, 64);
    assert(llvm_lower_coro_promise_alloc(nullptr, nonvoid_f, dummy_sz, 0, &alloc_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_coro_promise_alloc(&descr, nullptr, dummy_sz, 0, &alloc_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_coro_promise_alloc(&descr, nonvoid_f, nullptr, 0, &alloc_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_coro_promise_alloc(&descr, nonvoid_f, dummy_sz, 0, nullptr) == llvm_gen_be_error_t::invalid_argument);

    llvm::BasicBlock* lpad_out = nullptr;
    assert(llvm_emit_coro_unhandled_exception_landing_pad(nullptr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nullptr, &lpad_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, nullptr) == llvm_gen_be_error_t::invalid_argument);
    descr.unhandled_exception_call = nullptr;
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::invalid_argument);
    descr.unhandled_exception_call = &unhandled_ex;
    descr.final_suspend_label = nullptr;
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::invalid_argument);

    assert(llvm_lower_coro_noop(nullptr) == llvm_gen_be_error_t::invalid_argument);
    llvm::Value* noop_hdl = nullptr;
    assert(llvm_lower_coro_noop(&noop_hdl) == llvm_gen_be_error_t::ok);
    assert(noop_hdl != nullptr);
  }

  printf("PASS\n");
  return 0;
}
