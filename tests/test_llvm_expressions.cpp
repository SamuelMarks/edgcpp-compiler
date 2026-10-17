/**
 * @file test_llvm_expressions.cpp
 * @brief Unit tests for the LLVM backend expressions lowering subsystem.
 * @details Validates function coverage of llvm_lower_expression.
 */

#include "llvm_gen_be_expr.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_ptrmem.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  /**
   * @brief Resolves and extracts the LLVM FunctionType from an EDG routine or pointer-to-routine type.
   * @details Unwraps pointers, references, and typerefs, lowering the underlying routine type.
   * @param[in] type The EDG AST type pointer representing the routine or function pointer.
   * @param[out] out_ty Receives the lowered LLVM FunctionType pointer on success.
   * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
   */
  llvm_gen_be_error_t get_llvm_function_type(a_type_ptr type, llvm::FunctionType** out_ty) noexcept;
  /**
   * @brief Emits an LLVM bitfield load from a container memory address.
   * @details Loads the bitfield storage unit and shifts/masks/sign-extends the extracted bits.
   * @param[in] base_ptr Pointer to the bitfield storage unit.
   * @param[in] field The EDG bitfield descriptor.
   * @param[in] expr_type The resulting expression type of the bitfield load.
   * @param[out] out_val Receives the extracted bitfield value.
   * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
   */
  llvm_gen_be_error_t llvm_emit_bitfield_load(llvm::Value* base_ptr, a_field_ptr field, a_type_ptr expr_type, llvm::Value** out_val) noexcept;
  /**
   * @brief Emits an LLVM bitfield store into a container memory address.
   * @details Reads-modifies-writes the bitfield storage unit with the masked value.
   * @param[in] base_ptr Pointer to the bitfield storage unit.
   * @param[in] val The new value to store into the bitfield.
   * @param[in] field The EDG bitfield descriptor.
   * @param[in] expr_type The expression type of the bitfield assignment.
   * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
   */
  llvm_gen_be_error_t llvm_emit_bitfield_store(llvm::Value* base_ptr, llvm::Value* val, a_field_ptr field, a_type_ptr expr_type) noexcept;

/**
 * @brief Stub implementation of `get_mangled_function_name`.
 * @details Returns `nullptr` for expression unit tests.
 * @param[in] routine The EDG routine pointer.
 * @return Always `nullptr`.
 */
char* get_mangled_function_name(a_routine_ptr routine) {
    (void)routine;
    return nullptr;
}

  // Stub implementations required to link test
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) {
    return (ty && ty->kind == tk_integer && ty->variant.integer.bool_type) ? TRUE : FALSE;
  }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) {
    return (ty && ty->alignment == 77) ? TRUE : FALSE;
  }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_expressions.cpp`.
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
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_expressions.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_expressions.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { static char buf[] = "_ZTIi"; return buf; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_expressions.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) {
      (void)x;
      if (type && type->kind == tk_typeref && type->variant.typeref.qualifiers != 0) {
          return type->variant.typeref.qualifiers;
      }
      return (type && type->size == 999) ? TQ_VOLATILE : 0;
  }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return (ty && ty->size == 555) ? TRUE : FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_expressions.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (con && con->source_corresp.decl_position.seq == 888) return llvm_gen_be_error_t::unsupported_expr;
    if (ty->isIntegerTy()) {
      uint64_t v = (con && con->source_corresp.decl_position.seq >= 10)
                       ? (con->source_corresp.decl_position.seq - 10)
                       : 1;
      *out_const = llvm::ConstantInt::get(ty, v);
    }
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  /**
   * @brief Test stub or helper for `type_has_nontrivial_destructor`.
   * @details Provides mock or helper behavior for `type_has_nontrivial_destructor` within `tests/test_llvm_expressions.cpp`.
   * @param[in] tp Parameter `tp` for `type_has_nontrivial_destructor`.
   * @return Result of `type_has_nontrivial_destructor` (a_boolean).
   */
  a_boolean type_has_nontrivial_destructor(a_type_ptr tp) {
      return (tp && tp->size == 777) ? TRUE : FALSE;
  }

  /**
   * @brief Test stub or helper for `llvm_emit_eh_cleanup`.
   * @details Provides mock or helper behavior for `llvm_emit_eh_cleanup` within `tests/test_llvm_expressions.cpp`.
   * @param[in] dtor Parameter `dtor` for `llvm_emit_eh_cleanup`.
   * @param[in] obj_ptr Parameter `obj_ptr` for `llvm_emit_eh_cleanup`.
   * @return Result of `llvm_emit_eh_cleanup` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept {
      if (dtor && obj_ptr) {
        ScopeCleanup sc = {};
        sc.dtor_routine = dtor;
        sc.object_ptr = obj_ptr;
        sc.is_active = true;
        be_state->cleanup_stack.cleanups.push_back(sc);
      }
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_get_or_create_current_unwind_dest` in `tests/test_llvm_expressions.cpp`.
   * @details Returns the top of `be_state->current_landing_pads` (or `nullptr`).
   * @param[out] out_unwind_bb Receives the active landing pad basic block.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_get_or_create_current_unwind_dest(llvm::BasicBlock** out_unwind_bb) noexcept {
      if (!out_unwind_bb) return llvm_gen_be_error_t::invalid_argument;
      *out_unwind_bb = be_state->current_landing_pads.empty() ? nullptr : be_state->current_landing_pads.back();
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_get_or_create_msvc_throw_info` in `tests/test_llvm_expressions.cpp`.
   * @details Creates or retrieves a `_TI1?AU...` global variable in the active module.
   * @param[in] thrown_type The EDG type of the thrown exception object.
   * @param[in] dtor_fn Optional destructor `llvm::Constant*`.
   * @param[out] out_throw_info Receives the `_TI...` `llvm::Constant*` global pointer.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_get_or_create_msvc_throw_info(
      a_type_ptr thrown_type,
      llvm::Constant* dtor_fn,
      llvm::Constant** out_throw_info) noexcept {
      (void)dtor_fn;
      if (!thrown_type || !out_throw_info) return llvm_gen_be_error_t::invalid_argument;
      const char* raw_tname = thrown_type->source_corresp.name ? thrown_type->source_corresp.name : "type";
      std::string ti_name = std::string("_TI1?AU") + raw_tname + "@@";
      llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(ti_name);
      if (!gv) {
          llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
          gv = new llvm::GlobalVariable(
              *be_state->module, i8_ty, true, llvm::GlobalValue::LinkOnceODRLinkage,
              llvm::ConstantInt::get(i8_ty, 0), ti_name);
      }
      *out_throw_info = gv;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_expressions.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] out_loc Parameter `out_loc` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** out_loc) noexcept {
    if (arg_1.seq == 888) return llvm_gen_be_error_t::unsupported_expr;
    if (out_loc) {
      if (arg_1.seq == 777 || !arg_2) *out_loc = nullptr;
      else *out_loc = llvm::DILocation::get(*be_state->context, arg_1.seq, 1, arg_2);
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `emit_dbg_value_for_variable`.
   * @details Provides mock or helper behavior for `emit_dbg_value_for_variable` within `tests/test_llvm_expressions.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `emit_dbg_value_for_variable`.
   * @param[in] arg_1 Parameter `arg_1` for `emit_dbg_value_for_variable`.
   * @param[in] arg_2 Parameter `arg_2` for `emit_dbg_value_for_variable`.
   * @param[in] arg_3 Parameter `arg_3` for `emit_dbg_value_for_variable`.
   * @param[in] arg_4 Parameter `arg_4` for `emit_dbg_value_for_variable`.
   * @param[in] arg_5 Parameter `arg_5` for `emit_dbg_value_for_variable`.
   * @param[in] arg_6 Parameter `arg_6` for `emit_dbg_value_for_variable`.
   * @return Result of `emit_dbg_value_for_variable` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t emit_dbg_value_for_variable(llvm_gen_be_debug_state_t* arg_0, a_variable* arg_1, llvm::Value* arg_2, llvm::Instruction* arg_3, llvm::BasicBlock* arg_4, const uint64_t* arg_5, size_t arg_6) noexcept {
    if (arg_1 && arg_1->alignment == 99) return llvm_gen_be_error_t::unsupported_expr;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `vtbl_decider_function_for_class`.
   * @details Provides mock or helper behavior for `vtbl_decider_function_for_class` within `tests/test_llvm_expressions.cpp`.
   * @param[in] class_type Parameter `class_type` for `vtbl_decider_function_for_class`.
   * @param[in] unknown Parameter `unknown` for `vtbl_decider_function_for_class`.
   * @return Result of `vtbl_decider_function_for_class` (a_routine_ptr).
   */
  a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type, a_boolean* unknown) {
      if (unknown) *unknown = FALSE;
      return nullptr;
  }
}


namespace edg {
  /**
   * @brief Test stub or helper for `mangled_vtbl_name`.
   * @details Provides mock or helper behavior for `mangled_vtbl_name` within `tests/test_llvm_expressions.cpp`.
   * @param[in] class_type Parameter `class_type` for `mangled_vtbl_name`.
   * @param[in] bcp Parameter `bcp` for `mangled_vtbl_name`.
   * @param[in] ctor_bcp Parameter `ctor_bcp` for `mangled_vtbl_name`.
   * @return Result of `mangled_vtbl_name` (char*).
   */
  char* mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) {
      if (bcp) return (char*)"_ZTCmock";
      return (char*)"_ZTVmock";
  }
  /**
   * @brief Test stub or helper for `mangled_virtual_table_table_name`.
   * @details Provides mock or helper behavior for `mangled_virtual_table_table_name` within `tests/test_llvm_expressions.cpp`.
   * @param[in] type Parameter `type` for `mangled_virtual_table_table_name`.
   * @return Result of `mangled_virtual_table_table_name` (char*).
   */
  char* mangled_virtual_table_table_name(a_type_ptr type) {
      return (char*)"_ZTTmock";
  }
}

/**
 * @brief Lowers a builtin call expression with the specified routine name, return type, and operands.
 * @details Constructs a synthetic `enk_operation` (`eok_call`) node targeting a routine named @p name
 *          and invokes `llvm_lower_call_expression`.
 * @param[in] name Builtin routine name string.
 * @param[in] ret_t EDG return type for the call expression.
 * @param[in] ops Initializer list of operand expression node pointers.
 * @return llvm_gen_be_error_t::ok on success, or an error code from `llvm_lower_call_expression`.
 */
static llvm_gen_be_error_t call_test_builtin(
    const char* name,
    a_type_ptr ret_t,
    std::initializer_list<an_expr_node*> ops) noexcept {
  a_routine br = {};
  br.source_corresp.name = const_cast<char*>(name);
  an_expr_node bop1 = {};
  bop1.kind = enk_routine;
  bop1.variant.routine.ptr = &br;
  std::vector<an_expr_node> local_ops;
  local_ops.reserve(ops.size());
  for (an_expr_node* op : ops) {
    local_ops.push_back(*op);
    local_ops.back().next = nullptr;
  }
  an_expr_node* prev = &bop1;
  for (size_t i = 0; i < local_ops.size(); ++i) {
    prev->next = &local_ops[i];
    prev = &local_ops[i];
  }
  an_expr_node cexpr = {};
  cexpr.kind = enk_operation;
  cexpr.type = ret_t;
  cexpr.variant.operation.kind = eok_call;
  cexpr.variant.operation.operands = &bop1;
  llvm::Value* out = nullptr;
  return llvm_lower_call_expression(&cexpr, &out);
}

/**
 * @brief Lowers a throw expression and resets the IRBuilder insertion point to a fresh basic block.
 * @details Calls `llvm_lower_expression` on @p te and creates a continuation basic block in @p fn.
 * @param[in] te Pointer to the throw expression node.
 * @param[in,out] fn Enclosing `llvm::Function` to append the post-throw basic block to.
 * @return llvm_gen_be_error_t::ok on success, or an error code from `llvm_lower_expression`.
 */
static llvm_gen_be_error_t run_test_throw(
    an_expr_node* te,
    llvm::Function* fn) noexcept {
  llvm::Value* val = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(te, &val);
  llvm::BasicBlock* next_bb = llvm::BasicBlock::Create(*be_state->context, "post.throw", fn);
  be_state->builder->SetInsertPoint(next_bb);
  return err;
}

/**
 * @brief Entry point for the expression lowering unit test suite.
 * @details Exercises lvalues, arithmetic, logical, cast, call, compound assignment, complex, and C++ expressions.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_expressions...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "test", state.module.get());
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*state.context, "entry", F);
  state.builder->SetInsertPoint(BB);

  llvm::Value* val = nullptr;
  a_type int_type;
  memset(&int_type, 0, sizeof(int_type));
  int_type.kind = tk_integer;
  int_type.variant.integer.int_kind = ik_int;

  a_variable dummy_var;
  memset(&dummy_var, 0, sizeof(dummy_var));
  dummy_var.type = &int_type;
  
  an_expr_node dummy_op1;
  memset(&dummy_op1, 0, sizeof(dummy_op1));
  dummy_op1.type = &int_type;
  dummy_op1.kind = enk_variable;
  dummy_op1.variant.variable.ptr = &dummy_var;
  
  llvm::Type* var_ll_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::AllocaInst* alloca = state.builder->CreateAlloca(var_ll_ty);
  state.local_vars[&dummy_var] = alloca;

  an_expr_node dummy_op2;
  memset(&dummy_op2, 0, sizeof(dummy_op2));
  dummy_op2.type = &int_type;
  dummy_op2.kind = enk_constant;

  dummy_op1.next = &dummy_op2;

  an_expr_node dummy_expr;
  memset(&dummy_expr, 0, sizeof(dummy_expr));
  dummy_expr.type = &int_type;
  dummy_expr.variant.operation.operands = &dummy_op1;

  // Simple test to hit 1 branch
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
  
  a_type unsigned_type;
  memset(&unsigned_type, 0, sizeof(unsigned_type));
  unsigned_type.kind = tk_integer;
  unsigned_type.variant.integer.int_kind = ik_unsigned_int;
  
  a_type float_type;
  memset(&float_type, 0, sizeof(float_type));
  float_type.kind = tk_float;
  float_type.variant.float_kind = fk_float;
  
  a_type pointer_type;
  memset(&pointer_type, 0, sizeof(pointer_type));
  pointer_type.kind = tk_pointer;
  pointer_type.variant.pointer.type = &int_type;

  a_type* types[] = { &int_type, &unsigned_type, &float_type, &pointer_type };

  // Create test loops to hit all operations
  for (a_type* ty : types) {
      dummy_op1.type = ty;
      dummy_op2.type = ty;
      // Only test basic operations that don't crash LLVM constant folder with dummy values
      for (int i = 0; i < 10; ++i) {
          memset(&dummy_expr, 0, sizeof(dummy_expr));
          dummy_expr.type = ty;
          dummy_expr.kind = enk_operation;
          dummy_expr.variant.operation.operands = &dummy_op1;
          dummy_expr.variant.operation.kind = static_cast<an_expr_operator_kind>(i);
          if (ty == &float_type || ty == &pointer_type) continue;
          if (llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok) { }
      }
  }

  // Test compound assignments
  an_expr_operator_kind compound_ops[] = {
    eok_add_assign, eok_subtract_assign, eok_multiply_assign, eok_divide_assign,
    eok_remainder_assign, eok_and_assign, eok_or_assign, eok_xor_assign,
    eok_shiftl_assign, eok_shiftr_assign, eok_padd_assign, eok_psubtract_assign,
    eok_bassign, eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr
  };

  for (an_expr_operator_kind op : compound_ops) {
      memset(&dummy_expr, 0, sizeof(dummy_expr));
      dummy_expr.type = &int_type;
      dummy_expr.kind = enk_operation;
      dummy_expr.variant.operation.operands = &dummy_op1;
      dummy_expr.variant.operation.kind = op;
      assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  }

  // Test complex numbers
  a_type complex_type;
  memset(&complex_type, 0, sizeof(complex_type));
  complex_type.kind = tk_complex;
  complex_type.variant.float_kind = fk_float;

  an_expr_node dummy_complex_op1;
  memset(&dummy_complex_op1, 0, sizeof(dummy_complex_op1));
  dummy_complex_op1.type = &complex_type;
  dummy_complex_op1.kind = enk_variable;
  
  a_variable dummy_complex_var;
  memset(&dummy_complex_var, 0, sizeof(dummy_complex_var));
  dummy_complex_var.type = &complex_type;
  dummy_complex_op1.variant.variable.ptr = &dummy_complex_var;
  
  llvm::Type* complex_ll_ty = nullptr;
  assert(get_llvm_type(&complex_type, &complex_ll_ty) == llvm_gen_be_error_t::ok);
  state.local_vars[&dummy_complex_var] = state.builder->CreateAlloca(complex_ll_ty);

  an_expr_node dummy_complex_op2;
  memset(&dummy_complex_op2, 0, sizeof(dummy_complex_op2));
  dummy_complex_op2.type = &complex_type;
  dummy_complex_op2.kind = enk_variable;
  dummy_complex_op2.variant.variable.ptr = &dummy_complex_var;
  dummy_complex_op1.next = &dummy_complex_op2;

  an_expr_operator_kind complex_ops[] = {
      eok_add, eok_subtract, eok_multiply, eok_divide
  };
  
  for (an_expr_operator_kind op : complex_ops) {
      memset(&dummy_expr, 0, sizeof(dummy_expr));
      dummy_expr.type = &complex_type;
      dummy_expr.kind = enk_operation;
      dummy_expr.variant.operation.operands = &dummy_complex_op1;
      dummy_expr.variant.operation.kind = op;
      assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  }

  // Test imaginary operators
  an_expr_operator_kind imag_ops[] = {
      eok_fjadd, eok_jfadd, eok_fjsubtract, eok_jfsubtract, eok_jmultiply, eok_jdivide
  };
  
  an_expr_node dummy_float_op1;
  memset(&dummy_float_op1, 0, sizeof(dummy_float_op1));
  dummy_float_op1.type = &float_type;
  dummy_float_op1.kind = enk_variable;
  a_variable dummy_float_var;
  memset(&dummy_float_var, 0, sizeof(dummy_float_var));
  dummy_float_var.type = &float_type;
  dummy_float_op1.variant.variable.ptr = &dummy_float_var;
  state.local_vars[&dummy_float_var] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));
  
  dummy_float_op1.next = &dummy_complex_op2; // For real op imag

  for (an_expr_operator_kind op : imag_ops) {
      memset(&dummy_expr, 0, sizeof(dummy_expr));
      dummy_expr.type = &complex_type;
      dummy_expr.kind = enk_operation;
      if (op == eok_fjadd || op == eok_fjsubtract || op == eok_jdivide) {
          dummy_expr.variant.operation.operands = &dummy_float_op1;
          dummy_float_op1.next = &dummy_complex_op2;
      } else {
          dummy_expr.variant.operation.operands = &dummy_complex_op1;
          dummy_complex_op1.next = &dummy_float_op1;
      }
      assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  }
  
  // Test spaceship operator
  a_type strong_ordering_type;
  memset(&strong_ordering_type, 0, sizeof(strong_ordering_type));
  strong_ordering_type.kind = tk_struct; // e.g. std::strong_ordering
  
  memset(&dummy_expr, 0, sizeof(dummy_expr));
  dummy_expr.type = &strong_ordering_type; // e.g. strong_ordering struct
  dummy_expr.kind = enk_operation;
  dummy_expr.variant.operation.operands = &dummy_op1; // integers
  dummy_expr.variant.operation.kind = eok_spaceship;
  dummy_op1.next = &dummy_op2;
  
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);

  // Test bitfield load/store
  a_field dummy_bf = {};
  dummy_bf.is_bit_field = TRUE;
  dummy_bf.type = &int_type;
  dummy_bf.bit_size = 5;
  dummy_bf.offset_bit_remainder = 3;
  dummy_bf.offset = 0;
  dummy_bf.bit_field_is_signed = TRUE;

  an_expr_node dot_expr = {};
  dot_expr.kind = enk_operation;
  dot_expr.type = &int_type;
  dot_expr.is_lvalue = TRUE;
  dot_expr.variant.operation.kind = eok_dot_field;
  dot_expr.variant.operation.operands = &dummy_op1;
  
  an_expr_node field_node = {};
  field_node.kind = enk_field;
  field_node.variant.field.ptr = &dummy_bf;
  dummy_op1.next = &field_node;
  
  // Test load
  assert(llvm_lower_expression(&dot_expr, &val) == llvm_gen_be_error_t::ok);
  
  // Test store (assign)
  an_expr_node assign_expr = {};
  assign_expr.kind = enk_operation;
  assign_expr.type = &int_type;
  assign_expr.variant.operation.kind = eok_assign;
  assign_expr.variant.operation.operands = &dot_expr;
  an_expr_node rhs_expr = {};
  rhs_expr.kind = enk_constant;
  rhs_expr.type = &int_type;
  dot_expr.next = &rhs_expr;
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

  // Test compound assign on bitfield
  assign_expr.variant.operation.kind = eok_add_assign;
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);



  // Test Compiler Builtins (Overflow/Clz/Expect)
  an_expr_node overflow_expr = {};
  overflow_expr.kind = enk_operation;
  overflow_expr.variant.operation.kind = eok_call;
  an_expr_node overflow_func = {};
  overflow_func.kind = enk_routine;
  a_routine overflow_routine = {};
  overflow_routine.source_corresp.name = "__builtin_sadd_overflow";
  overflow_func.variant.routine.ptr = &overflow_routine;
  
  an_expr_node op_a = {}; op_a.kind = enk_constant; op_a.type = &int_type;
  an_expr_node op_b = {}; op_b.kind = enk_constant; op_b.type = &int_type;
  an_expr_node op_res = {}; op_res.kind = enk_constant; op_res.type = &pointer_type;
  
  overflow_func.next = &op_a;
  op_a.next = &op_b;
  op_b.next = &op_res;
  
  overflow_expr.variant.operation.operands = &overflow_func;
  assert(llvm_lower_expression(&overflow_expr, &val) == llvm_gen_be_error_t::ok);


  // Test Atomics
  an_expr_node atomic_expr = {};
  atomic_expr.kind = enk_operation;
  atomic_expr.variant.operation.kind = eok_call;
  an_expr_node atomic_func = {};
  atomic_func.kind = enk_routine;
  a_routine atomic_routine = {};
  atomic_routine.source_corresp.name = "__atomic_thread_fence";
  atomic_func.variant.routine.ptr = &atomic_routine;
  
  an_expr_node atomic_arg = {}; atomic_arg.kind = enk_constant; atomic_arg.type = &int_type;
  atomic_func.next = &atomic_arg;
  atomic_expr.variant.operation.operands = &atomic_func;
  
  assert(llvm_lower_expression(&atomic_expr, &val) == llvm_gen_be_error_t::ok);

  // Test SIMD builtins
  an_expr_node simd_expr = {};
  simd_expr.kind = enk_operation;
  simd_expr.variant.operation.kind = eok_call;
  an_expr_node simd_func = {};
  simd_func.kind = enk_routine;
  a_routine simd_routine = {};
  simd_routine.source_corresp.name = "__builtin_ia32_sse_add_ps";
  simd_func.variant.routine.ptr = &simd_routine;
  simd_func.next = nullptr; // Re-use vector argument
  simd_expr.variant.operation.operands = &simd_func;
  
  assert(llvm_lower_expression(&simd_expr, &val) == llvm_gen_be_error_t::ok);

  
  // Test SIMD builtins (AVX, SVE, Shuffle, Convert)
  {
      an_expr_node op1 = {};
      op1.kind = enk_constant;
      a_type op_ty = {};
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
      op_ty.kind = tk_vector;
      op_ty.variant.vector.element_type = &int_type;
#else
      op_ty.kind = tk_array;
      op_ty.variant.array.element_type = &int_type;
      op_ty.variant.array.variant.number_of_elements = 4;
#endif
      op_ty.size = 16;
      op1.type = &op_ty;
      
      an_expr_node op2 = {};
      op2.kind = enk_constant;
      op2.type = &op_ty;

      an_expr_node mask = {};
      mask.kind = enk_constant;
      mask.type = &int_type;
      a_constant mask_const = {};
      mask_const.kind = ck_integer;
      mask_const.variant.integer_value = 0;
      mask.variant.constant.ptr = &mask_const;

      an_expr_node simd_avx = {};
      simd_avx.kind = enk_operation;
      simd_avx.variant.operation.kind = eok_call;
      an_expr_node simd_avx_func = {};
      simd_avx_func.kind = enk_routine;
      a_routine simd_avx_routine = {};
      simd_avx_routine.source_corresp.name = "__builtin_ia32_avx_add_pd_256";
      simd_avx_func.variant.routine.ptr = &simd_avx_routine;
      simd_avx_func.next = &op1;
      op1.next = &op2;
      op2.next = nullptr;
      simd_avx.variant.operation.operands = &simd_avx_func;
      simd_avx.type = &op_ty;
      assert(llvm_lower_expression(&simd_avx, &val) == llvm_gen_be_error_t::ok);

      an_expr_node simd_sve = {};
      simd_sve.kind = enk_operation;
      simd_sve.variant.operation.kind = eok_call;
      an_expr_node simd_sve_func = {};
      simd_sve_func.kind = enk_routine;
      a_routine simd_sve_routine = {};
      simd_sve_routine.source_corresp.name = "__builtin_sve_add_nxv4i32";
      simd_sve_func.variant.routine.ptr = &simd_sve_routine;
      simd_sve_func.next = &op1;
      op1.next = &op2;
      op2.next = nullptr;
      simd_sve.variant.operation.operands = &simd_sve_func;
      simd_sve.type = &op_ty;
      assert(llvm_lower_expression(&simd_sve, &val) == llvm_gen_be_error_t::ok);

      // Shufflevector
      an_expr_node simd_shuf = {};
      simd_shuf.kind = enk_operation;
      simd_shuf.variant.operation.kind = eok_call;
      an_expr_node simd_shuf_func = {};
      simd_shuf_func.kind = enk_routine;
      a_routine simd_shuf_routine = {};
      simd_shuf_routine.source_corresp.name = "__builtin_shufflevector";
      simd_shuf_func.variant.routine.ptr = &simd_shuf_routine;
      
      simd_shuf_func.next = &op1;
      op1.next = &op2;
      op2.next = &mask;
      mask.next = nullptr;
      
      simd_shuf.variant.operation.operands = &simd_shuf_func;
      simd_shuf.type = &op_ty;
      assert(llvm_lower_expression(&simd_shuf, &val) == llvm_gen_be_error_t::ok);

      // Convertvector
      an_expr_node simd_conv = {};
      simd_conv.kind = enk_operation;
      simd_conv.variant.operation.kind = eok_call;
      an_expr_node simd_conv_func = {};
      simd_conv_func.kind = enk_routine;
      a_routine simd_conv_routine = {};
      simd_conv_routine.source_corresp.name = "__builtin_convertvector";
      simd_conv_func.variant.routine.ptr = &simd_conv_routine;
      
      simd_conv_func.next = &op1;
      op1.next = &op2;
      op2.next = nullptr; // only 2 args

      a_type fp_vec_ty = {};
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
      fp_vec_ty.kind = tk_vector;
      a_type fp_type = {};
      fp_type.kind = tk_float;
      fp_type.variant.float_kind = fk_float;
      fp_vec_ty.variant.vector.element_type = &fp_type;
#else
      fp_vec_ty.kind = tk_array;
      a_type fp_type = {};
      fp_type.kind = tk_float;
      fp_type.variant.float_kind = fk_float;
      fp_vec_ty.variant.array.element_type = &fp_type;
      fp_vec_ty.variant.array.variant.number_of_elements = 4;
#endif
      fp_vec_ty.size = 16;
      simd_conv.type = &fp_vec_ty;
      op2.type = &fp_vec_ty; // destination type argument
      
      simd_conv.variant.operation.operands = &simd_conv_func;
      assert(llvm_lower_expression(&simd_conv, &val) == llvm_gen_be_error_t::ok);
  }
  // test_stmt_expr();
  // Test GNU statement expression
  an_expr_node stmt_expr = {};
  stmt_expr.kind = enk_statement;
  stmt_expr.type = &int_type;
  a_statement comp_stmt = {};
  comp_stmt.kind = stmk_empty;
  stmt_expr.variant.statement = &comp_stmt;
  
  assert(llvm_lower_expression(&stmt_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_setjmp
  an_expr_node setjmp_expr = {};
  setjmp_expr.kind = enk_operation;
  setjmp_expr.variant.operation.kind = eok_call;
  
  an_expr_node setjmp_func = {};
  setjmp_func.kind = enk_routine;
  a_routine setjmp_routine = {};
  setjmp_routine.source_corresp.name = "__builtin_setjmp";
  setjmp_func.variant.routine.ptr = &setjmp_routine;
  
  an_expr_node setjmp_arg = {};
  setjmp_arg.kind = enk_constant;
  setjmp_arg.type = &int_type; // Stub pointer
  setjmp_func.next = &setjmp_arg;
  setjmp_expr.variant.operation.operands = &setjmp_func;
  
  assert(llvm_lower_expression(&setjmp_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_longjmp
  an_expr_node longjmp_expr = {};
  longjmp_expr.kind = enk_operation;
  longjmp_expr.variant.operation.kind = eok_call;
  
  an_expr_node longjmp_func = {};
  longjmp_func.kind = enk_routine;
  a_routine longjmp_routine = {};
  longjmp_routine.source_corresp.name = "__builtin_longjmp";
  longjmp_func.variant.routine.ptr = &longjmp_routine;
  longjmp_func.next = &setjmp_arg;
  longjmp_expr.variant.operation.operands = &longjmp_func;
  
  assert(llvm_lower_expression(&longjmp_expr, &val) == llvm_gen_be_error_t::ok);

  // Test spanning bitfield (e.g. 20 bits starting at bit offset 4, spans 3 bytes)
  dummy_bf.offset_bit_remainder = 4;
  dummy_bf.bit_size = 20;
  dummy_bf.bit_field_is_signed = FALSE;
  assert(llvm_lower_expression(&dot_expr, &val) == llvm_gen_be_error_t::ok);
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

  // Test volatile bitfield atomic operations
  a_type vol_int_type = int_type;
  vol_int_type.size = 999;
  dot_expr.type = &vol_int_type;
  assert(llvm_lower_expression(&dot_expr, &val) == llvm_gen_be_error_t::ok);
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

  // Test complex multiplication
  dummy_complex_op2.kind = enk_variable;
  dummy_complex_op2.variant.variable.ptr = &dummy_complex_var;
  dummy_op1.next = &dummy_complex_op2;
  dummy_expr.type = &complex_type;
  dummy_expr.kind = enk_operation;
  dummy_expr.variant.operation.operands = &dummy_op1;
  dummy_expr.variant.operation.kind = eok_multiply;
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);

  // Test complex division
  dummy_expr.variant.operation.kind = eok_divide;
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  
  // Test complex conjugation
  dummy_op1.next = nullptr;
  dummy_expr.variant.operation.kind = eok_xconj;
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_cabs
  an_expr_node cabs_expr = {};
  cabs_expr.kind = enk_operation;
  cabs_expr.variant.operation.kind = eok_call;
  an_expr_node cabs_func = {};
  cabs_func.kind = enk_routine;
  a_routine cabs_routine = {};
  cabs_routine.source_corresp.name = "__builtin_cabs";
  cabs_func.variant.routine.ptr = &cabs_routine;
  cabs_func.next = &dummy_op1;
  cabs_expr.variant.operation.operands = &cabs_func;
  assert(llvm_lower_expression(&cabs_expr, &val) == llvm_gen_be_error_t::ok);




  // Test Pointer-to-Member Invocation (Itanium ABI representation)
  an_expr_node ptrmem_expr = {};
  ptrmem_expr.kind = enk_operation;
  ptrmem_expr.variant.operation.kind = eok_points_to_member_call;
  
  an_expr_node ptrmem_op1 = {};
  ptrmem_op1.kind = enk_variable;
  a_type ptr_to_member_ty = {};
  ptr_to_member_ty.kind = tk_ptr_to_member;
  
  a_type rout_ty = {};
  rout_ty.kind = tk_routine;
  ptr_to_member_ty.variant.ptr_to_member.type = &rout_ty;
  ptrmem_op1.type = &ptr_to_member_ty;
  
  a_variable ptrmem_var = {};
  ptrmem_var.type = &ptr_to_member_ty;
  ptrmem_op1.variant.variable.ptr = &ptrmem_var;
  
  llvm::Type* int64_ty = llvm::Type::getInt64Ty(*state.context);
  llvm::Type* ptrmem_llvm_ty = llvm::StructType::get(*state.context, {llvm::PointerType::getUnqual(*state.context), int64_ty});
  state.local_vars[&ptrmem_var] = state.builder->CreateAlloca(ptrmem_llvm_ty);
  
  // the 'this' argument
  an_expr_node this_arg = {};
  this_arg.kind = enk_constant;
  this_arg.type = &pointer_type;
  ptrmem_op1.next = &this_arg;
  
  ptrmem_expr.variant.operation.operands = &ptrmem_op1;
  
  llvm_gen_be_error_t e2 = llvm_lower_expression(&ptrmem_expr, &val);
  if (e2 != llvm_gen_be_error_t::ok) { printf("ERR: %d\\n", (int)e2); }
  assert(e2 == llvm_gen_be_error_t::ok);




#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
  // Test Vector Type Lowering
  a_type vec_ty = {};
  vec_ty.kind = tk_vector;
  vec_ty.size = 16;
  a_vector_type_supplement vec_supp = {};
  vec_supp.element_type = &int_type;
  vec_ty.variant.vector.extra_info = &vec_supp;
  
  llvm::Type* vec_llvm_ty = nullptr;
  assert(get_llvm_type(&vec_ty, &vec_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(vec_llvm_ty != nullptr);
  assert(vec_llvm_ty->isVectorTy());
  
  // Test vector subscript
  an_expr_node vec_subscript = {};
  vec_subscript.kind = enk_operation;
  vec_subscript.variant.operation.kind = eok_subscript;
  vec_subscript.type = &int_type;
  
  an_expr_node vec_op1 = {};
  vec_op1.kind = enk_variable;
  a_variable vec_var = {};
  vec_var.type = &vec_ty;
  vec_op1.variant.variable.ptr = &vec_var;
  state.local_vars[&vec_var] = state.builder->CreateAlloca(vec_llvm_ty);
  
  an_expr_node vec_op2 = {};
  vec_op2.kind = enk_constant;
  vec_op2.type = &int_type;
  
  vec_op1.next = &vec_op2;
  vec_subscript.variant.operation.operands = &vec_op1;
  
  assert(llvm_lower_expression(&vec_subscript, &val) == llvm_gen_be_error_t::ok);
#endif

  // Phase 3: Pointer-to-member subsystem unit tests
  a_type struct_type = {};
  struct_type.kind = tk_struct;
  llvm::Type* struct_llvm_ty = llvm::Type::getInt8Ty(*state.context);

  a_type pm_func_type = {};
  pm_func_type.kind = tk_ptr_to_member;
  a_type routine_type = {};
  routine_type.kind = tk_routine;
  pm_func_type.variant.ptr_to_member.type = &routine_type;
  pm_func_type.variant.ptr_to_member.class_of_which_a_member = &struct_type;

  a_type pm_data_type = {};
  pm_data_type.kind = tk_ptr_to_member;
  pm_data_type.variant.ptr_to_member.type = &int_type;
  pm_data_type.variant.ptr_to_member.class_of_which_a_member = &struct_type;

  // 1. Type lowering
  llvm::Type* pm_func_llvm_ty = nullptr;
  assert(llvm_type_from_ptr_to_member(nullptr, &pm_func_llvm_ty) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_type_from_ptr_to_member(&pm_func_type, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_type_from_ptr_to_member(&int_type, &pm_func_llvm_ty) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_type_from_ptr_to_member(&pm_func_type, &pm_func_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(pm_func_llvm_ty && pm_func_llvm_ty->isStructTy());

  llvm::Type* pm_data_llvm_ty = nullptr;
  assert(llvm_type_from_ptr_to_member(&pm_data_type, &pm_data_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(pm_data_llvm_ty && pm_data_llvm_ty->isIntegerTy());

  // 2. Constant evaluation
  llvm::Constant* pm_const = nullptr;
  assert(llvm_const_from_ptr_to_member(nullptr, pm_func_llvm_ty, &pm_const) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_ptr_to_member(nullptr, nullptr, &pm_const) == llvm_gen_be_error_t::invalid_argument);

  a_constant null_pm_con = {};
  null_pm_con.kind = ck_integer; // non ck_ptr_to_member represents null
  assert(llvm_const_from_ptr_to_member(&null_pm_con, pm_func_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);
  assert(pm_const != nullptr);

  assert(llvm_const_from_ptr_to_member(&null_pm_con, pm_data_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);
  assert(pm_const != nullptr);

  a_constant pm_data_con = {};
  pm_data_con.kind = ck_ptr_to_member;
  pm_data_con.variant.ptr_to_member.is_function_ptr = FALSE;
  a_field field_obj = {};
  field_obj.offset = 16;
  pm_data_con.variant.ptr_to_member.variant.field = &field_obj;
  assert(llvm_const_from_ptr_to_member(&pm_data_con, pm_data_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);
  assert(pm_const != nullptr);

  // 3. PM Call lowering
  an_expr_node pm_call_expr = {};
  pm_call_expr.kind = enk_operation;
  pm_call_expr.variant.operation.kind = eok_dot_pm_call;
  a_constant pm_fn_con = {};
  pm_fn_con.kind = ck_ptr_to_member;
  pm_fn_con.type = &pm_func_type;
  an_expr_node pm_op1 = {};
  pm_op1.kind = enk_constant;
  pm_op1.type = &pm_func_type;
  pm_op1.variant.constant.ptr = &pm_fn_con;
  an_expr_node pm_op2 = {};
  pm_op2.kind = enk_variable;
  pm_op2.type = &struct_type;
  pm_op2.is_lvalue = TRUE;
  a_variable pm_obj_var = {};
  pm_obj_var.type = &struct_type;
  pm_op2.variant.variable.ptr = &pm_obj_var;
  state.local_vars[&pm_obj_var] = state.builder->CreateAlloca(struct_llvm_ty);

  pm_op1.next = &pm_op2;
  pm_call_expr.variant.operation.operands = &pm_op1;

  llvm::Value* pm_call_val = nullptr;
  assert(llvm_lower_pm_call(nullptr, &pm_call_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_call(&pm_call_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_call(&pm_call_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
  assert(pm_call_val != nullptr);

  // 4. PM Field access lowering
  a_constant pm_int_con = {};
  pm_int_con.kind = ck_integer;
  pm_int_con.type = &int_type;
  pm_int_con.variant.integer_value = 8;

  an_expr_node pm_data_op = {};
  pm_data_op.kind = enk_constant;
  pm_data_op.type = &int_type;
  pm_data_op.variant.constant.ptr = &pm_int_con;
  pm_data_op.next = &pm_op2;

  an_expr_node pm_field_expr = {};
  pm_field_expr.kind = enk_operation;
  pm_field_expr.variant.operation.kind = eok_pm_field;
  pm_field_expr.variant.operation.operands = &pm_data_op;

  llvm::Value* pm_field_val = nullptr;
  assert(llvm_lower_pm_field_access(nullptr, &pm_field_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_field_access(&pm_field_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_field_access(&pm_field_expr, &pm_field_val) == llvm_gen_be_error_t::ok);
  assert(pm_field_val != nullptr);

  // 5. PM Cast lowering
  an_expr_node pm_cast_expr = {};
  pm_cast_expr.kind = enk_operation;
  pm_cast_expr.variant.operation.kind = eok_pm_base_class_cast;
  pm_cast_expr.variant.operation.operands = &pm_op1;

  llvm::Value* pm_cast_val = nullptr;
  assert(llvm_lower_pm_cast(nullptr, &pm_cast_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_cast(&pm_cast_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
  assert(pm_cast_val != nullptr);

  // 6. PM Comparison lowering
  an_expr_node pm_cmp_expr = {};
  pm_cmp_expr.kind = enk_operation;
  pm_cmp_expr.variant.operation.kind = eok_eq;
  an_expr_node pm_cmp_op2 = pm_op1;
  pm_op1.next = &pm_cmp_op2;
  pm_cmp_expr.variant.operation.operands = &pm_op1;

  llvm::Value* pm_cmp_val = nullptr;
  assert(llvm_lower_pm_comparison(nullptr, &pm_cmp_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_comparison(&pm_cmp_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_comparison(&pm_cmp_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);
  assert(pm_cmp_val != nullptr);

  // 7. enk_new_delete lowering
  an_expr_node new_expr = {};
  new_expr.kind = enk_new_delete;
  new_expr.type = &pointer_type;
  
  a_routine new_routine = {};
  new_routine.source_corresp.name = "operator new";
  new_routine.type = &int_type; // dummy function type
  
  a_new_delete_supplement nd = {};
  nd.is_new = 1;
  nd.type = &int_type;
  nd.routine = &new_routine;
  new_expr.variant.new_delete = &nd;

  llvm::Value* new_val = nullptr;
  assert(llvm_lower_expression(&new_expr, &new_val) == llvm_gen_be_error_t::ok);
  assert(new_val != nullptr);

  a_type void_type = {};
  void_type.kind = tk_void;

  an_expr_node delete_expr = {};
  delete_expr.kind = enk_new_delete;
  delete_expr.type = &void_type;
  
  a_routine delete_routine = {};
  delete_routine.source_corresp.name = "operator delete";
  delete_routine.type = &int_type;
  
  a_new_delete_supplement delete_nd = {};
  delete_nd.is_new = 0;
  delete_nd.type = &int_type;
  delete_nd.routine = &delete_routine;
  an_expr_node del_arg = {};
  del_arg.kind = enk_constant; // dummy ptr
  del_arg.type = &pointer_type;
  delete_nd.arg = &del_arg;
  delete_expr.variant.new_delete = &delete_nd;

  llvm::Value* delete_val = nullptr;
  assert(llvm_lower_expression(&delete_expr, &delete_val) == llvm_gen_be_error_t::ok);
  assert(delete_val == nullptr);

  // 8. enk_new_delete array lowering
  an_expr_node new_array_expr = {};
  new_array_expr.kind = enk_new_delete;
  new_array_expr.type = &pointer_type;
  
  a_routine new_array_routine = {};
  new_array_routine.source_corresp.name = "operator new[]";
  new_array_routine.type = &int_type;
  
  a_new_delete_supplement nd_array = {};
  nd_array.is_new = 1;
  nd_array.type = &int_type;
  nd_array.routine = &new_array_routine;
  
  an_expr_node array_size_expr = {};
  array_size_expr.kind = enk_constant;
  array_size_expr.type = &int_type; // 10 elements
  nd_array.number_of_elements = &array_size_expr;
  new_array_expr.variant.new_delete = &nd_array;

  llvm::Value* new_array_val = nullptr;
  assert(llvm_lower_expression(&new_array_expr, &new_array_val) == llvm_gen_be_error_t::ok);
  assert(new_array_val != nullptr);

  an_expr_node delete_array_expr = {};
  delete_array_expr.kind = enk_new_delete;
  delete_array_expr.type = &void_type;
  
  a_routine delete_array_routine = {};
  delete_array_routine.source_corresp.name = "operator delete[]";
  delete_array_routine.type = &int_type;
  
  a_new_delete_supplement delete_array_nd = {};
  delete_array_nd.is_new = 0;
  delete_array_nd.array_delete = 1;
  delete_array_nd.type = &int_type;
  delete_array_nd.routine = &delete_array_routine;
  an_expr_node del_array_arg = {};
  del_array_arg.kind = enk_constant; // dummy ptr
  del_array_arg.type = &pointer_type;
  delete_array_nd.arg = &del_array_arg;
  delete_array_expr.variant.new_delete = &delete_array_nd;

  llvm::Value* delete_array_val = nullptr;
  assert(llvm_lower_expression(&delete_array_expr, &delete_array_val) == llvm_gen_be_error_t::ok);
  assert(delete_array_val == nullptr);

  // 9. enk_new_delete placement new lowering
  an_expr_node placement_new_expr = {};
  placement_new_expr.kind = enk_new_delete;
  placement_new_expr.type = &pointer_type;
  
  a_routine placement_new_routine = {};
  placement_new_routine.source_corresp.name = "operator new";
  placement_new_routine.type = &pointer_type; // actually returns void*
  
  a_new_delete_supplement nd_placement = {};
  nd_placement.is_new = 1;
  nd_placement.placement_new = 1;
  nd_placement.type = &int_type;
  nd_placement.routine = &placement_new_routine;
  
  an_expr_node placement_arg = {};
  placement_arg.kind = enk_constant;
  placement_arg.type = &pointer_type;
  nd_placement.arg = &placement_arg;
  
  placement_new_expr.variant.new_delete = &nd_placement;

  llvm::Value* placement_new_val = nullptr;
  assert(llvm_lower_expression(&placement_new_expr, &placement_new_val) == llvm_gen_be_error_t::ok);
  assert(placement_new_val != nullptr);

  // 10. enk_new_delete class-specific lowering
  an_expr_node class_new_expr = {};
  class_new_expr.kind = enk_new_delete;
  class_new_expr.type = &pointer_type;
  
  a_routine class_new_routine = {};
  class_new_routine.source_corresp.name = "_ZN1TnwEm"; // mangled T::operator new
  class_new_routine.type = &pointer_type;
  
  a_new_delete_supplement nd_class = {};
  nd_class.is_new = 1;
  nd_class.type = &int_type;
  nd_class.routine = &class_new_routine;
  class_new_expr.variant.new_delete = &nd_class;

  llvm::Value* class_new_val = nullptr;
  assert(llvm_lower_expression(&class_new_expr, &class_new_val) == llvm_gen_be_error_t::ok);
  assert(class_new_val != nullptr);

  an_expr_node class_delete_expr = {};
  class_delete_expr.kind = enk_new_delete;
  class_delete_expr.type = &void_type;
  
  a_routine class_delete_routine = {};
  class_delete_routine.source_corresp.name = "_ZN1TdlEPv"; // mangled T::operator delete
  class_delete_routine.type = &int_type;
  
  a_new_delete_supplement class_delete_nd = {};
  class_delete_nd.is_new = 0;
  class_delete_nd.type = &int_type;
  class_delete_nd.routine = &class_delete_routine;
  an_expr_node class_del_arg = {};
  class_del_arg.kind = enk_constant; // dummy ptr
  class_del_arg.type = &pointer_type;
  class_delete_nd.arg = &class_del_arg;
  class_delete_expr.variant.new_delete = &class_delete_nd;

  llvm::Value* class_delete_val = nullptr;
  assert(llvm_lower_expression(&class_delete_expr, &class_delete_val) == llvm_gen_be_error_t::ok);
  assert(class_delete_val == nullptr);

  // 11. enk_new_delete sized and aligned deallocation lowering
  an_expr_node sized_delete_expr = {};
  sized_delete_expr.kind = enk_new_delete;
  sized_delete_expr.type = &void_type;
  
  a_routine sized_delete_routine = {};
  sized_delete_routine.source_corresp.name = "_ZdlPvmSt11align_val_t"; // mangled operator delete(void*, size_t, std::align_val_t)
  sized_delete_routine.type = &int_type;
  
  a_new_delete_supplement sized_delete_nd = {};
  sized_delete_nd.is_new = 0;
  sized_delete_nd.aligned_version = 1;
  sized_delete_nd.type = &int_type;
  sized_delete_nd.routine = &sized_delete_routine;
  an_expr_node sized_del_arg = {};
  sized_del_arg.kind = enk_constant; // dummy ptr
  sized_del_arg.type = &pointer_type;
  
  an_expr_node size_arg = {};
  size_arg.kind = enk_constant; // size
  size_arg.type = &int_type;
  sized_del_arg.next = &size_arg;
  
  an_expr_node align_arg = {};
  align_arg.kind = enk_constant; // align
  align_arg.type = &int_type;
  size_arg.next = &align_arg;
  
  sized_delete_nd.arg = &sized_del_arg;
  sized_delete_expr.variant.new_delete = &sized_delete_nd;

  llvm::Value* sized_delete_val = nullptr;
  assert(llvm_lower_expression(&sized_delete_expr, &sized_delete_val) == llvm_gen_be_error_t::ok);
  assert(sized_delete_val == nullptr);

  // 12. enk_temp_init lowering
  an_expr_node temp_init_expr = {};
  temp_init_expr.kind = enk_temp_init;
  temp_init_expr.type = &int_type;
  temp_init_expr.is_lvalue = 0; // rvalue so it should load
  
  a_dynamic_init temp_dip = {};
  temp_dip.kind = dik_expression;
  an_expr_node temp_init_val_expr = {};
  temp_init_val_expr.kind = enk_constant;
  temp_init_val_expr.type = &int_type;
  temp_dip.variant.expression = &temp_init_val_expr;
  
  temp_init_expr.variant.init.dynamic_init = &temp_dip;

  llvm::Value* temp_init_val = nullptr;
  assert(llvm_lower_expression(&temp_init_expr, &temp_init_val) == llvm_gen_be_error_t::ok);
  assert(temp_init_val != nullptr);
  // As it's an rvalue, we expect a load instruction
  assert(llvm::isa<llvm::LoadInst>(temp_init_val));

  // 13. enk_temp_init with destructor (lifetime)
  an_expr_node temp_dtor_expr = {};
  temp_dtor_expr.kind = enk_temp_init;
  temp_dtor_expr.type = &int_type;
  temp_dtor_expr.is_lvalue = 1; // lvalue so it should yield pointer
  
  a_routine temp_dtor_routine = {};
  temp_dtor_routine.source_corresp.name = "~Temp";
  temp_dtor_routine.type = &void_type;
  
  a_dynamic_init temp_dtor_dip = {};
  temp_dtor_dip.kind = dik_expression;
  temp_dtor_dip.variant.expression = &temp_init_val_expr;
  temp_dtor_dip.destructor = &temp_dtor_routine;
  temp_dtor_expr.variant.init.dynamic_init = &temp_dtor_dip;
  
  llvm::Value* temp_dtor_val = nullptr;
  assert(llvm_lower_expression(&temp_dtor_expr, &temp_dtor_val) == llvm_gen_be_error_t::ok);
  assert(temp_dtor_val != nullptr);
  assert(llvm::isa<llvm::AllocaInst>(temp_dtor_val));

  // 14. enk_object_lifetime lowering
  an_expr_node obj_lifetime_expr = {};
  obj_lifetime_expr.kind = enk_object_lifetime;
  obj_lifetime_expr.type = &int_type;
  
  an_object_lifetime obj_lifetime = {};
  obj_lifetime_expr.variant.object_lifetime.ptr = &obj_lifetime;
  obj_lifetime_expr.variant.object_lifetime.expr = &temp_dtor_expr; // Reuse the temp_dtor_expr which pushes a cleanup
  
  llvm::Value* obj_lifetime_val = nullptr;
  assert(llvm_lower_expression(&obj_lifetime_expr, &obj_lifetime_val) == llvm_gen_be_error_t::ok);
  assert(obj_lifetime_val != nullptr);

  // 15. eok_negate and eok_unary_plus
  an_expr_node negate_expr = {};
  negate_expr.kind = enk_operation;
  negate_expr.type = &int_type;
  negate_expr.variant.operation.kind = eok_negate;
  negate_expr.variant.operation.operands = &temp_init_val_expr; // reused integer constant
  
  llvm::Value* negate_val = nullptr;
  assert(llvm_lower_expression(&negate_expr, &negate_val) == llvm_gen_be_error_t::ok);
  assert(negate_val != nullptr);
  
  an_expr_node unary_plus_expr = {};
  unary_plus_expr.kind = enk_operation;
  unary_plus_expr.type = &int_type;
  unary_plus_expr.variant.operation.kind = eok_unary_plus;
  unary_plus_expr.variant.operation.operands = &temp_init_val_expr;
  
  llvm::Value* unary_plus_val = nullptr;
  assert(llvm_lower_expression(&unary_plus_expr, &unary_plus_val) == llvm_gen_be_error_t::ok);
  assert(unary_plus_val != nullptr);

  // 16. eok_comma operator
  an_expr_node comma_expr = {};
  comma_expr.kind = enk_operation;
  comma_expr.type = &float_type;
  comma_expr.variant.operation.kind = eok_comma;
  
  an_expr_node comma_op1 = {};
  comma_op1.kind = enk_constant;
  comma_op1.type = &int_type;
  
  an_expr_node comma_op2 = {};
  comma_op2.kind = enk_constant;
  comma_op2.type = &float_type;
  
  comma_op1.next = &comma_op2;
  comma_expr.variant.operation.operands = &comma_op1;
  
  llvm::Value* comma_val = nullptr;
  assert(llvm_lower_expression(&comma_expr, &comma_val) == llvm_gen_be_error_t::ok);
  assert(comma_val != nullptr);
  assert(comma_val->getType()->isFloatTy());

  // Test static member and vacuous destructor calls
  an_expr_node static_expr;
  memset(&static_expr, 0, sizeof(static_expr));
  static_expr.kind = enk_operation;
  static_expr.type = &int_type;
  
  an_expr_node static_op1;
  memset(&static_op1, 0, sizeof(static_op1));
  static_op1.kind = enk_constant;
  static_op1.type = &int_type;

  an_expr_node static_op2;
  memset(&static_op2, 0, sizeof(static_op2));
  static_op2.kind = enk_constant;
  static_op2.type = &int_type;

  static_op1.next = &static_op2;
  static_expr.variant.operation.operands = &static_op1;

  // eok_dot_static
  static_expr.variant.operation.kind = eok_dot_static;
  llvm::Value* static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val != nullptr);

  // eok_points_to_static
  static_expr.variant.operation.kind = eok_points_to_static;
  static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val != nullptr);

  // eok_dot_vacuous_destructor_call
  static_expr.variant.operation.kind = eok_dot_vacuous_destructor_call;
  static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val == nullptr);

  // eok_points_to_vacuous_destructor_call
  static_expr.variant.operation.kind = eok_points_to_vacuous_destructor_call;
  static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val == nullptr);

  // Test unsupported CLI operations
  static_expr.variant.operation.kind = eok_handle_to;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);
  static_expr.variant.operation.kind = eok_box;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);
  static_expr.variant.operation.kind = eok_unbox;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);
  static_expr.variant.operation.kind = eok_cli_subscript;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);

  // Test enk_braced_init_list
  an_expr_node braced_expr;
  memset(&braced_expr, 0, sizeof(braced_expr));
  braced_expr.kind = enk_braced_init_list;
  braced_expr.type = &int_type;
  braced_expr.variant.braced_init_list = &static_op1; // Reusing a simple constant from above
  llvm::Value* braced_val = nullptr;
  assert(llvm_lower_expression(&braced_expr, &braced_val) == llvm_gen_be_error_t::ok);
  assert(braced_val != nullptr);

  // Test enk_reuse_value
  an_expr_node reuse_expr;
  memset(&reuse_expr, 0, sizeof(reuse_expr));
  reuse_expr.kind = enk_reuse_value;
  reuse_expr.type = &int_type;
  
  a_dynamic_init dyn_init;
  memset(&dyn_init, 0, sizeof(dyn_init));
  dyn_init.kind = dik_expression;
  dyn_init.variant.expression = &static_op1;
  reuse_expr.variant.reused_value_init = &dyn_init;
  
  llvm::Value* reuse_val = nullptr;
  assert(llvm_lower_expression(&reuse_expr, &reuse_val) == llvm_gen_be_error_t::ok);
  assert(reuse_val != nullptr);

  // Test enk_object_lifetime
  an_expr_node lifetime_expr;
  memset(&lifetime_expr, 0, sizeof(lifetime_expr));
  lifetime_expr.kind = enk_object_lifetime;
  lifetime_expr.type = &int_type;
  lifetime_expr.variant.object_lifetime.expr = &static_op1;
  llvm::Value* lifetime_val = nullptr;
  assert(llvm_lower_expression(&lifetime_expr, &lifetime_val) == llvm_gen_be_error_t::ok);
  assert(lifetime_val != nullptr);

  // Test enk_fold, enk_pack_index, enk_c11_generic (empty/unsupported and valid)
  an_expr_node unsupported_expr;
  memset(&unsupported_expr, 0, sizeof(unsupported_expr));
  unsupported_expr.kind = enk_fold;
  unsupported_expr.type = &int_type;
  llvm::Value* unsup_val = nullptr;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  an_expr_node fold_op1 = static_op1;
  an_expr_node fold_op2 = static_op1;
  fold_op1.next = &fold_op2;
  fold_op2.next = nullptr;
  unsupported_expr.variant.fold.operands = &fold_op1;
  unsupported_expr.variant.fold.operator_token = tok_plus;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::ok);
  assert(unsup_val != nullptr);

  unsupported_expr.kind = enk_pack_index;
  memset(&unsupported_expr.variant, 0, sizeof(unsupported_expr.variant));
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  unsupported_expr.variant.pack_index.expr = &fold_op1;
  unsupported_expr.variant.pack_index.index_expr = &static_op1;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::ok);
  assert(unsup_val != nullptr);

  unsupported_expr.kind = enk_c11_generic;
  memset(&unsupported_expr.variant, 0, sizeof(unsupported_expr.variant));
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  unsupported_expr.variant.c11_generic.result = &static_op1;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::ok);
  assert(unsup_val != nullptr);

  // Test enk_reuse_value with dik_zero and dik_constant
  a_dynamic_init zero_init = {};
  zero_init.kind = dik_zero;
  reuse_expr.variant.reused_value_init = &zero_init;
  assert(llvm_lower_expression(&reuse_expr, &reuse_val) == llvm_gen_be_error_t::ok);
  assert(reuse_val != nullptr);

  a_dynamic_init const_init = {};
  const_init.kind = dik_constant;
  a_constant con_dummy = {};
  const_init.variant.constant.ptr = &con_dummy;
  reuse_expr.variant.reused_value_init = &const_init;
  assert(llvm_lower_expression(&reuse_expr, &reuse_val) == llvm_gen_be_error_t::ok);
  assert(reuse_val != nullptr);

  // Test eok_va_start, eok_va_arg, eok_va_end, eok_va_copy
  a_type ptr_type = {};
  ptr_type.kind = tk_pointer;
  ptr_type.size = 8;
  ptr_type.variant.pointer.type = &int_type;

  a_variable va_var = {};
  va_var.source_corresp.name = (char*)"ap";
  va_var.type = &ptr_type;
  llvm::AllocaInst* va_alloca = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context), nullptr, "ap");
  state.local_vars[&va_var] = va_alloca;

  an_expr_node va_var_node = {};
  va_var_node.kind = enk_variable;
  va_var_node.type = &ptr_type;
  va_var_node.variant.variable.ptr = &va_var;

  an_expr_node va_op_expr = {};
  va_op_expr.kind = enk_operation;
  va_op_expr.type = &int_type;
  va_op_expr.variant.operation.operands = &va_var_node;

  llvm::Value* va_out = nullptr;
  va_op_expr.variant.operation.kind = eok_va_start_single_operand;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
  va_op_expr.variant.operation.kind = eok_va_arg;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
  va_op_expr.variant.operation.kind = eok_va_end;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);

  an_expr_node va_src_node = va_var_node;
  va_var_node.next = &va_src_node;
  va_op_expr.variant.operation.kind = eok_va_copy;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
  va_var_node.next = nullptr;

  // Test _Complex equality/inequality and scalar-to-_Complex / _Complex-to-scalar casts
  a_type cplx_ty = {};
  cplx_ty.kind = tk_complex;
  cplx_ty.size = 8;
  cplx_ty.variant.float_kind = fk_float;

  a_variable cplx_var = {};
  cplx_var.type = &cplx_ty;
  llvm::Type* cplx_llvm_ty = nullptr;
  assert(get_llvm_type(&cplx_ty, &cplx_llvm_ty) == llvm_gen_be_error_t::ok);
  state.local_vars[&cplx_var] = state.builder->CreateAlloca(cplx_llvm_ty, nullptr, "cplx_v");

  an_expr_node cplx_op1 = {};
  cplx_op1.kind = enk_variable;
  cplx_op1.type = &cplx_ty;
  cplx_op1.variant.variable.ptr = &cplx_var;
  an_expr_node cplx_op2 = cplx_op1;
  cplx_op1.next = &cplx_op2;

  an_expr_node cplx_cmp = {};
  cplx_cmp.kind = enk_operation;
  cplx_cmp.type = &int_type;
  cplx_cmp.variant.operation.kind = eok_eq;
  cplx_cmp.variant.operation.operands = &cplx_op1;
  llvm::Value* cplx_cmp_val = nullptr;
  assert(llvm_lower_expression(&cplx_cmp, &cplx_cmp_val) == llvm_gen_be_error_t::ok);
  cplx_cmp.variant.operation.kind = eok_ne;
  assert(llvm_lower_expression(&cplx_cmp, &cplx_cmp_val) == llvm_gen_be_error_t::ok);
  cplx_op1.next = nullptr;

  an_expr_node to_cplx_cast = {};
  to_cplx_cast.kind = enk_operation;
  to_cplx_cast.type = &cplx_ty;
  to_cplx_cast.variant.operation.kind = eok_cast;
  to_cplx_cast.variant.operation.operands = &static_op1;
  llvm::Value* cplx_cast_val = nullptr;
  assert(llvm_lower_expression(&to_cplx_cast, &cplx_cast_val) == llvm_gen_be_error_t::ok);

  an_expr_node from_cplx_cast = {};
  from_cplx_cast.kind = enk_operation;
  from_cplx_cast.type = &float_type;
  from_cplx_cast.variant.operation.kind = eok_cast;
  from_cplx_cast.variant.operation.operands = &cplx_op1;
  assert(llvm_lower_expression(&from_cplx_cast, &cplx_cast_val) == llvm_gen_be_error_t::ok);

  // --- Comprehensive coverage tests for llvm_gen_be_expr.cpp ---
  {
    a_type bad_ty = {};
    bad_ty.kind = static_cast<a_type_kind>(250);
    a_type void_ty = {};
    void_ty.kind = tk_void;
    a_type i8_ty = {};
    i8_ty.kind = tk_integer;
    i8_ty.size = 1;
    i8_ty.variant.integer.int_kind = ik_char;
    a_type i16_ty = {};
    i16_ty.kind = tk_integer;
    i16_ty.size = 2;
    i16_ty.variant.integer.int_kind = ik_short;
    a_type i64_ty = {};
    i64_ty.kind = tk_integer;
    i64_ty.size = 8;
    i64_ty.variant.integer.int_kind = ik_long;
    a_type u32_ty = {};
    u32_ty.kind = tk_integer;
    u32_ty.size = 4;
    u32_ty.variant.integer.int_kind = ik_unsigned_int;
    int_kind_is_signed[ik_int] = TRUE;
    int_kind_is_signed[ik_long] = TRUE;
    int_kind_is_signed[ik_short] = TRUE;
    int_kind_is_signed[ik_char] = TRUE;
    int_kind_is_signed[ik_unsigned_int] = FALSE;

    a_type double_ty = {};
    double_ty.kind = tk_float;
    double_ty.size = 8;
    double_ty.variant.float_kind = fk_double;

    a_type cplx_double_ty = {};
    cplx_double_ty.kind = tk_complex;
    cplx_double_ty.size = 16;
    cplx_double_ty.variant.float_kind = fk_double;

    // 1. get_llvm_function_type coverage
    llvm::FunctionType* fn_ty_out = nullptr;
    assert(get_llvm_function_type(nullptr, &fn_ty_out) == llvm_gen_be_error_t::ok);
    assert(get_llvm_function_type(&int_type, &fn_ty_out) == llvm_gen_be_error_t::ok);

    a_routine_type_supplement rts_fn = {};
    a_param_type p1_fn = {};
    p1_fn.type = &int_type;
    rts_fn.param_type_list = &p1_fn;
    a_type rout_ty = {};
    rout_ty.kind = tk_routine;
    rout_ty.variant.routine.return_type = &int_type;
    rout_ty.variant.routine.extra_info = &rts_fn;
    a_type tref_rout_ty = {};
    tref_rout_ty.kind = tk_typeref;
    tref_rout_ty.variant.typeref.type = &rout_ty;
    a_type ptr_rout_ty = {};
    ptr_rout_ty.kind = tk_pointer;
    ptr_rout_ty.size = 8;
    ptr_rout_ty.variant.pointer.type = &tref_rout_ty;
    assert(get_llvm_function_type(&ptr_rout_ty, &fn_ty_out) == llvm_gen_be_error_t::ok);

    a_type ref_rout_ty = {};
    ref_rout_ty.kind = tk_typeref;
    ref_rout_ty.size = 555; // triggers is_reference_type
    ref_rout_ty.variant.pointer.type = &rout_ty;
    assert(get_llvm_function_type(&ref_rout_ty, &fn_ty_out) == llvm_gen_be_error_t::ok);

    state.type_cache.erase(&rout_ty);
    rout_ty.variant.routine.return_type = &bad_ty;
    assert(get_llvm_function_type(&rout_ty, &fn_ty_out) != llvm_gen_be_error_t::ok);
    state.type_cache.erase(&rout_ty);
    rout_ty.variant.routine.return_type = nullptr;
    p1_fn.type = &bad_ty;
    assert(get_llvm_function_type(&rout_ty, &fn_ty_out) != llvm_gen_be_error_t::ok);
    state.type_cache.erase(&rout_ty);
    p1_fn.type = &int_type;
    assert(get_llvm_function_type(&rout_ty, &fn_ty_out) == llvm_gen_be_error_t::ok);
    state.type_cache.erase(&rout_ty);
    rout_ty.variant.routine.return_type = &int_type;

    // 2. Bitfield load & store (volatile, MSVC, spanning, wider/narrower dest_ty, big-endian, shift==0)
    a_type vol_int_ty = int_type;
    vol_int_ty.size = 999; // triggers TQ_VOLATILE in f_get_type_qualifiers
    a_field bf_f = {};
    bf_f.is_bit_field = TRUE;
    bf_f.type = &int_type;
    int_type.size = 4;
    bf_f.offset_bit_remainder = 0; // shift == 0
    bf_f.bit_size = 5;
    llvm::Value* bf_ptr = state.builder->CreateAlloca(llvm::Type::getInt64Ty(*state.context));
    llvm::Value* bf_val = nullptr;
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &bad_ty, &bf_val) != llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &vol_int_ty, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_store(bf_ptr, bf_val, &bf_f, &int_type) == llvm_gen_be_error_t::ok);
    bf_f.offset_bit_remainder = 2; // shift > 0
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &i64_ty, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &i8_ty, &bf_val) == llvm_gen_be_error_t::ok);
    targ_little_endian = FALSE;
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &int_type, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_store(bf_ptr, bf_val, &bf_f, &int_type) == llvm_gen_be_error_t::ok);
    targ_little_endian = TRUE;
    bf_f.offset_bit_remainder = 4;
    bf_f.bit_size = 6; // 10 bits -> 16-bit container
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &u32_ty, &bf_val) == llvm_gen_be_error_t::ok);
    bf_f.offset_bit_remainder = 4;
    bf_f.bit_size = 16; // 20 bits -> 32-bit container
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &int_type, &bf_val) == llvm_gen_be_error_t::ok);

    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &int_type, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_store(bf_ptr, bf_val, &bf_f, &vol_int_ty) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    assert(llvm_emit_bitfield_store(bf_ptr, state.builder->getInt64(3), &bf_f, &int_type) == llvm_gen_be_error_t::ok);

    // 3. Complex arithmetic & comparisons & casts (float and double and int-complex)
    a_variable cplx_d_var = {};
    cplx_d_var.type = &cplx_double_ty;
    llvm::Type* cplx_d_llvm_ty = nullptr;
    assert(get_llvm_type(&cplx_double_ty, &cplx_d_llvm_ty) == llvm_gen_be_error_t::ok);
    state.local_vars[&cplx_d_var] = state.builder->CreateAlloca(cplx_d_llvm_ty);

    an_expr_node cd_op1 = {};
    cd_op1.kind = enk_variable;
    cd_op1.type = &cplx_double_ty;
    cd_op1.variant.variable.ptr = &cplx_d_var;
    an_expr_node cd_op2 = cd_op1;
    cd_op1.next = &cd_op2;

    an_expr_node cd_arith = {};
    cd_arith.kind = enk_operation;
    cd_arith.type = &cplx_double_ty;
    cd_arith.variant.operation.operands = &cd_op1;
    for (an_expr_operator_kind k : {eok_negate, eok_add, eok_subtract, eok_multiply, eok_divide, eok_xconj}) {
      cd_arith.variant.operation.kind = k;
      assert(llvm_lower_arithmetic_expression(&cd_arith, &val) == llvm_gen_be_error_t::ok);
    }
    cplx_op1.next = &cplx_op2;
    cd_arith.type = &cplx_ty;
    cd_arith.variant.operation.operands = &cplx_op1;
    for (an_expr_operator_kind k : {eok_multiply, eok_divide, eok_xconj}) {
      cd_arith.variant.operation.kind = k;
      assert(llvm_lower_arithmetic_expression(&cd_arith, &val) == llvm_gen_be_error_t::ok);
    }
    cplx_op1.next = nullptr;
    cd_op1.next = nullptr;

    // Complex to complex cast, float to complex cast, complex to double cast
    an_expr_node c2c_cast = {};
    c2c_cast.kind = enk_operation;
    c2c_cast.type = &cplx_double_ty;
    c2c_cast.variant.operation.kind = eok_cast;
    c2c_cast.variant.operation.operands = &cplx_op1;
    assert(llvm_lower_cast_expression(&c2c_cast, &val) == llvm_gen_be_error_t::ok);
    c2c_cast.type = &double_ty;
    assert(llvm_lower_cast_expression(&c2c_cast, &val) == llvm_gen_be_error_t::ok);

    an_expr_node flt_const_node = {};
    flt_const_node.kind = enk_constant;
    flt_const_node.type = &float_type;
    c2c_cast.type = &cplx_double_ty;
    c2c_cast.variant.operation.operands = &flt_const_node;
    assert(llvm_lower_cast_expression(&c2c_cast, &val) == llvm_gen_be_error_t::ok);

    // Integer complex equality/inequality (2-element int struct)
    a_field ic_f1 = {}, ic_f2 = {};
    ic_f1.type = &int_type;
    ic_f2.type = &int_type;
    ic_f2.offset = 4;
    ic_f1.next = &ic_f2;
    a_type ic_struct_ty = {};
    ic_struct_ty.kind = tk_struct;
    ic_struct_ty.size = 8;
    ic_struct_ty.variant.class_struct_union.field_list = &ic_f1;
    llvm::Type* ic_llvm_ty = nullptr;
    assert(get_llvm_type(&ic_struct_ty, &ic_llvm_ty) == llvm_gen_be_error_t::ok);
    a_variable ic_var = {};
    ic_var.type = &ic_struct_ty;
    state.local_vars[&ic_var] = state.builder->CreateAlloca(ic_llvm_ty);
    an_expr_node ic_op1 = {}, ic_op2 = {};
    ic_op1.kind = enk_variable;
    ic_op1.type = &ic_struct_ty;
    ic_op1.variant.variable.ptr = &ic_var;
    ic_op2 = ic_op1;
    ic_op1.next = &ic_op2;
    an_expr_node ic_cmp = {};
    ic_cmp.kind = enk_operation;
    ic_cmp.type = &int_type;
    ic_cmp.variant.operation.operands = &ic_op1;
    ic_cmp.variant.operation.kind = eok_eq;
    assert(llvm_lower_logical_expression(&ic_cmp, &val) == llvm_gen_be_error_t::ok);
    ic_cmp.variant.operation.kind = eok_ne;
    assert(llvm_lower_logical_expression(&ic_cmp, &val) == llvm_gen_be_error_t::ok);

    // eok_real_part and eok_imag_part lvalue & rvalue on struct, scalar, void, null
    an_expr_node ri_expr = {};
    ri_expr.kind = enk_operation;
    ri_expr.type = &float_type;
    ri_expr.variant.operation.operands = &cplx_op1;
    ri_expr.variant.operation.kind = eok_real_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    ri_expr.variant.operation.kind = eok_imag_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);

    ri_expr.variant.operation.operands = &dummy_op1;
    dummy_op1.next = nullptr;
    ri_expr.type = &int_type;
    ri_expr.variant.operation.kind = eok_real_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    ri_expr.variant.operation.kind = eok_imag_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    ri_expr.type = &void_ty;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    ri_expr.type = &bad_ty;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) != llvm_gen_be_error_t::ok);
    ri_expr.type = nullptr;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 4. Multi-dimensional VLA subscript lvalue
    a_vla_dimension vla_dim = {};
    state.vla_dimension_values[&vla_dim] = state.builder->getInt32(4);
    a_type vla_inner_arr = {};
    vla_inner_arr.kind = tk_array;
    vla_inner_arr.variant.array.element_type = &int_type;
    vla_inner_arr.variant.array.bound_is_zero = FALSE;
    vla_inner_arr.variant.array.variant.number_of_elements = 8;
    a_type vla_zero_arr = {};
    vla_zero_arr.kind = tk_array;
    vla_zero_arr.variant.array.element_type = &vla_inner_arr;
    vla_zero_arr.variant.array.bound_is_zero = TRUE;
    a_type vla_outer_arr = {};
    vla_outer_arr.kind = tk_array;
    vla_outer_arr.variant.array.is_vla = TRUE;
    vla_outer_arr.variant.array.has_assoc_vla_dimension = TRUE;
    vla_outer_arr.variant.array.element_type = &vla_zero_arr;
    state.array_to_vla_dim[&vla_outer_arr] = &vla_dim;

    an_expr_node vla_ptr_op = {};
    vla_ptr_op.kind = enk_variable;
    vla_ptr_op.type = &ptr_type;
    vla_ptr_op.variant.variable.ptr = &va_var;
    an_expr_node vla_idx_op = {};
    vla_idx_op.kind = enk_constant;
    vla_idx_op.type = &int_type;
    vla_ptr_op.next = &vla_idx_op;

    an_expr_node vla_sub = {};
    vla_sub.kind = enk_operation;
    vla_sub.type = &vla_outer_arr;
    vla_sub.variant.operation.kind = eok_subscript;
    vla_sub.variant.operation.operands = &vla_ptr_op;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;

    // 5. All target ABIs + builtins + atomics in llvm_lower_call_expression
    const char* target_triples[] = {
      "x86_64-pc-windows-msvc",
      "aarch64-unknown-linux-gnu",
      "i686-unknown-linux-gnu",
      "armv7-unknown-linux-gnueabihf",
      "thumbv7-unknown-linux-gnueabihf",
      "riscv64-unknown-linux-gnu",
      "loongarch64-unknown-linux-gnu",
      "mips64-unknown-linux-gnu",
      "powerpc64le-unknown-linux-gnu",
      "s390x-ibm-linux",
      "sparcv9-sun-solaris",
      "wasm32-unknown-unknown",
      "x86_64-unknown-linux-gnu"
    };

    a_routine ext_fn_rout = {};
    ext_fn_rout.source_corresp.name = (char*)"abi_test_fn";
    ext_fn_rout.type = &rout_ty;
    an_expr_node fn_callee_op = {};
    fn_callee_op.kind = enk_routine;
    fn_callee_op.type = &ptr_rout_ty;
    fn_callee_op.variant.routine.ptr = &ext_fn_rout;
    an_expr_node fn_arg1 = {};
    fn_arg1.kind = enk_constant;
    fn_arg1.type = &int_type;
    fn_callee_op.next = &fn_arg1;
    an_expr_node abi_call_expr = {};
    abi_call_expr.kind = enk_operation;
    abi_call_expr.type = &int_type;
    abi_call_expr.variant.operation.kind = eok_call;
    abi_call_expr.variant.operation.operands = &fn_callee_op;

    int abi_idx = 0;
    static char abi_fnames[16][64];
    for (const char* t : target_triples) {
      snprintf(abi_fnames[abi_idx], sizeof(abi_fnames[abi_idx]), "abi_test_fn_%d", abi_idx);
      ext_fn_rout.source_corresp.name = abi_fnames[abi_idx++];
      state.module->setTargetTriple(llvm::Triple(t));
      assert(llvm_lower_call_expression(&abi_call_expr, &val) == llvm_gen_be_error_t::ok);
      // Also test va_start, va_arg, va_end, va_copy on every target triple
      va_var_node.next = nullptr;
      va_op_expr.variant.operation.kind = eok_va_start;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_op_expr.variant.operation.kind = eok_va_arg;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_op_expr.variant.operation.kind = eok_va_end;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_var_node.next = &va_src_node;
      va_op_expr.variant.operation.kind = eok_va_copy;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_var_node.next = nullptr;
    }

    // Also test resolve_va_list_ptr when operand is not in local_vars
    an_expr_node va_expr_ptr = {};
    va_expr_ptr.kind = enk_operation;
    va_expr_ptr.type = &ptr_type;
    va_expr_ptr.variant.operation.kind = eok_address_of;
    va_expr_ptr.variant.operation.operands = &va_var_node;
    va_op_expr.variant.operation.operands = &va_expr_ptr;
    va_op_expr.variant.operation.kind = eok_va_end;
    assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
    va_op_expr.variant.operation.operands = &va_var_node;

    // Test all builtins in llvm_lower_call_expression
    a_constant c_ord[7] = {};
    an_expr_node n_ord[7] = {};
    for (int i = 0; i < 7; ++i) {
      c_ord[i].source_corresp.decl_position.seq = 10 + i; // evaluates to i
      n_ord[i].kind = enk_constant;
      n_ord[i].type = &int_type;
      n_ord[i].variant.constant.ptr = &c_ord[i];
    }

    an_expr_node i32_arg1 = n_ord[1], i32_arg2 = n_ord[2], i32_arg3 = n_ord[3];
    an_expr_node i8_arg = {};
    i8_arg.kind = enk_constant;
    i8_arg.type = &i8_ty;
    an_expr_node i64_arg = {};
    i64_arg.kind = enk_constant;
    i64_arg.type = &i64_ty;
    an_expr_node u32_arg = {};
    u32_arg.kind = enk_constant;
    u32_arg.type = &u32_ty;
    an_expr_node flt_arg1 = flt_const_node, flt_arg2 = flt_const_node, flt_arg3 = flt_const_node;
    an_expr_node ptr_arg1 = va_var_node, ptr_arg2 = va_var_node, ptr_arg3 = va_var_node;

    for (const char* u_name : {
           "__builtin_sqrt", "__builtin_sin", "__builtin_cos", "__builtin_exp",
           "__builtin_log", "__builtin_round", "__builtin_trunc", "__builtin_floor",
           "__builtin_ceil", "__builtin_cabs"}) {
      assert(call_test_builtin(u_name, &float_type, {&flt_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_cabs", &float_type, {&cplx_op1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_pow", &float_type, {&flt_arg1, &flt_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_copysign", &float_type, {&flt_arg1, &flt_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_fma", &float_type, {&flt_arg1, &flt_arg2, &flt_arg3}) == llvm_gen_be_error_t::ok);

    for (const char* b_name : {
           "__builtin_bswap32", "__builtin_popcount", "__builtin_constant_p",
           "__builtin_clz", "__builtin_ctz", "__builtin_ffs", "__builtin_clrsb",
           "__builtin_parity",
           "__builtin_frame_address", "__builtin_return_address",
           "__builtin_alloca", "__builtin_assume", "setjmp", "longjmp",
           "__builtin___emutls_get_address", "__builtin_strlen"}) {
      assert(call_test_builtin(b_name, &int_type, {&i32_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_launder", &ptr_type, {&ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_dump_struct", &int_type, {&ptr_arg1, &ptr_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_prefetch", &void_ty, {&ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_prefetch", &void_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_prefetch", &void_ty, {&ptr_arg1, &i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_clear_cache", &void_ty, {&ptr_arg1, &ptr_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_readcyclecounter", &i64_ty, {}) == llvm_gen_be_error_t::ok);
    for (const char* fp_cls_name : {
           "__builtin_isnan", "__builtin_isinf", "__builtin_isfinite",
           "__builtin_isnormal", "__builtin_signbit", "__builtin_signbitf"}) {
      assert(call_test_builtin(fp_cls_name, &int_type, {&flt_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_fpclassify", &int_type,
                 {&i32_arg1, &i32_arg2, &i32_arg3, &n_ord[4], &n_ord[5], &flt_arg1}) == llvm_gen_be_error_t::ok);
    a_constant c_16 = {};
    c_16.source_corresp.decl_position.seq = 10 + 16;
    an_expr_node n_16 = {};
    n_16.kind = enk_constant;
    n_16.type = &int_type;
    n_16.variant.constant.ptr = &c_16;
    assert(call_test_builtin("__builtin___emutls_get_address", &ptr_type, {&ptr_arg1}) == llvm_gen_be_error_t::ok); // second call reuses existing function
    assert(call_test_builtin("__builtin_expect", &i64_ty, {&i64_arg, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_expect_with_probability", &int_type, {&i32_arg1, &i32_arg2, &flt_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_alloca_with_align", &ptr_type, {&i32_arg1, &n_16}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_alloca_with_align", &ptr_type, {&i32_arg1, &dummy_op1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rotateleft32", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rotateright32", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_object_size", &i64_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_dynamic_object_size", &i64_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_memcpy", &ptr_type, {&ptr_arg1, &ptr_arg2, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_memmove", &ptr_type, {&ptr_arg1, &ptr_arg2, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_memset", &ptr_type, {&ptr_arg1, &i32_arg1, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strcmp", &int_type, {&ptr_arg1, &ptr_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strchr", &ptr_type, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strchr", &ptr_type, {&ptr_arg1, &i8_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strchr", &ptr_type, {&ptr_arg1, &i64_arg}) == llvm_gen_be_error_t::ok);

    // Overflow builtins (signed & unsigned, add/sub/mul)
    for (const char* ov_name : {
           "__builtin_add_overflow", "__builtin_sub_overflow", "__builtin_mul_overflow",
           "__builtin_sadd_overflow", "__builtin_uadd_overflow",
           "__builtin_ssub_overflow", "__builtin_usub_overflow",
           "__builtin_smul_overflow", "__builtin_umul_overflow"}) {
      assert(call_test_builtin(ov_name, &int_type, {&i32_arg1, &i32_arg2, &ptr_arg1}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin(ov_name, &int_type, {&u32_arg, &u32_arg, &ptr_arg1}) == llvm_gen_be_error_t::ok);
    }

    // Target builtins: x86 avx512, avx2, avx, amx, neon, sve, rvv, vsetvli, vsetvlimax
    for (const char* tb_name : {
           "__builtin_ia32_avx512_mask", "__builtin_ia32_avx2_paddb",
           "__builtin_ia32_avx_vzeroall", "__builtin_ia32_amx_tilezero",
           "__builtin_ia32_tileloadd64", "__builtin_ia32_tdpbssd",
           "__builtin_ia32_pause", "__builtin_neon_vadd",
           "__builtin_sve_svadd", "__builtin_rvv_vadd"}) {
      assert(call_test_builtin(tb_name, &int_type, {&i32_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_rvv_vsetvli", &i64_ty, {&i64_arg, &i64_arg, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rvv_vsetvli", &int_type, {&i32_arg1, &i32_arg2, &i32_arg3}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rvv_vsetvlimax", &i64_ty, {&i64_arg, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rvv_vsetvlimax", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);

    // Atomics and __sync builtins
    for (int i = 0; i < 7; ++i) {
      assert(call_test_builtin("__atomic_thread_fence", &void_ty, {&n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_signal_fence", &void_ty, {&n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_always_lock_free", &int_type, {&n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_load_n", &int_type, {&ptr_arg1, &n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_store_n", &void_ty, {&ptr_arg1, &i32_arg1, &n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_exchange_n", &int_type, {&ptr_arg1, &i32_arg1, &n_ord[i]}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("riscv64-unknown-linux-gnu"));
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("aarch64-unknown-linux-gnu"));
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&dummy_op1}) == llvm_gen_be_error_t::ok);

    assert(call_test_builtin("__builtin_atomic_wait", &void_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_atomic_wait", &void_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok); // reuse existing fn
    assert(call_test_builtin("__builtin_atomic_notify", &void_ty, {&ptr_arg1}) == llvm_gen_be_error_t::ok);

    assert(call_test_builtin("__atomic_load", &void_ty, {&ptr_arg1, &ptr_arg2, &n_ord[2]}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_store", &void_ty, {&ptr_arg1, &ptr_arg2, &n_ord[3]}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_exchange", &void_ty, {&ptr_arg1, &ptr_arg2, &ptr_arg3, &n_ord[4]}) == llvm_gen_be_error_t::ok);
    // Also test __atomic_load / __atomic_store / __atomic_exchange on float pointer and aggregate struct pointer
    {
      a_type flt_ptr_t = {}; flt_ptr_t.kind = tk_pointer; flt_ptr_t.size = 8; flt_ptr_t.variant.pointer.type = &float_type;
      state.type_cache[&flt_ptr_t] = llvm::PointerType::getUnqual(*state.context);
      an_expr_node flt_ptr_arg = ptr_arg2; flt_ptr_arg.type = &flt_ptr_t;
      assert(call_test_builtin("__atomic_load", &void_ty, {&ptr_arg1, &flt_ptr_arg, &n_ord[2]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_store", &void_ty, {&ptr_arg1, &flt_ptr_arg, &n_ord[3]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_exchange", &void_ty, {&ptr_arg1, &flt_ptr_arg, &ptr_arg3, &n_ord[4]}) == llvm_gen_be_error_t::ok);

      a_type agg_ptr_t = {}; agg_ptr_t.kind = tk_pointer; agg_ptr_t.size = 8; agg_ptr_t.variant.pointer.type = &struct_type;
      state.type_cache[&agg_ptr_t] = llvm::PointerType::getUnqual(*state.context);
      an_expr_node agg_ptr_arg = ptr_arg2; agg_ptr_arg.type = &agg_ptr_t;
      assert(call_test_builtin("__atomic_load", &void_ty, {&ptr_arg1, &agg_ptr_arg, &n_ord[2]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_store", &void_ty, {&ptr_arg1, &agg_ptr_arg, &n_ord[3]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_exchange", &void_ty, {&ptr_arg1, &agg_ptr_arg, &ptr_arg3, &n_ord[4]}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__atomic_compare_exchange_n", &int_type,
                 {&ptr_arg1, &ptr_arg2, &i32_arg1, &i32_arg2, &n_ord[4], &n_ord[2]}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_compare_exchange_n", &int_type,
                 {&ptr_arg1, &ptr_arg2, &flt_arg1, &i32_arg2, &n_ord[5], &n_ord[5]}) == llvm_gen_be_error_t::ok);

    assert(call_test_builtin("__sync_synchronize", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_lock_test_and_set", &int_type, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_lock_release", &void_ty, {&ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_val_compare_and_swap", &int_type, {&ptr_arg1, &i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_bool_compare_and_swap", &int_type, {&ptr_arg1, &i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);

    for (const char* rmw_name : {
           "__atomic_fetch_add", "__atomic_fetch_sub", "__atomic_fetch_and",
           "__atomic_fetch_or", "__atomic_fetch_xor", "__atomic_fetch_nand",
           "__atomic_fetch_min", "__atomic_fetch_max",
           "__sync_add_and_fetch", "__sync_sub_and_fetch", "__sync_and_and_fetch",
           "__sync_or_and_fetch", "__sync_xor_and_fetch", "__sync_nand_and_fetch"}) {
      assert(call_test_builtin(rmw_name, &int_type, {&ptr_arg1, &i32_arg1, &n_ord[5]}) == llvm_gen_be_error_t::ok);
    }
    for (const char* fp_rmw_name : {
           "__atomic_fetch_add", "__atomic_fetch_sub",
           "__atomic_fetch_min", "__atomic_fetch_max",
           "__sync_add_and_fetch", "__sync_sub_and_fetch"}) {
      assert(call_test_builtin(fp_rmw_name, &float_type, {&ptr_arg1, &flt_arg1, &n_ord[5]}) == llvm_gen_be_error_t::ok);
    }

    // 6. Debug location saving in llvm_lower_expression & emit_dbg_value_for_variable in eok_assign
    llvm::DIBuilder di_b(*state.module);
    llvm_gen_be_debug_state_t dbg_st = {};
    dbg_st.builder = &di_b;
    llvm::DIFile* di_file = di_b.createFile("expr.c", ".");
    dbg_st.compile_unit = di_b.createCompileUnit(llvm::dwarf::DW_LANG_C99, di_file, "edg", false, "", 0);
    dbg_st.scope_stack.push_back(di_file);
    state.dbg_state = &dbg_st;

    an_expr_node assign_expr = {};
    assign_expr.kind = enk_operation;
    assign_expr.type = &int_type;
    assign_expr.position.seq = 10;
    assign_expr.variant.operation.kind = eok_assign;
    dummy_op1.is_lvalue = TRUE;
    dummy_op1.next = &i32_arg1;
    i32_arg1.next = nullptr;
    assign_expr.variant.operation.operands = &dummy_op1;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

    assign_expr.position.seq = 777; // null loc
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    assign_expr.position.seq = 888; // error in get_di_location (ignored by DILocSaver)
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    dummy_var.alignment = 99; // error in emit_dbg_value_for_variable
    assert(llvm_lower_expression(&assign_expr, &val) != llvm_gen_be_error_t::ok);
    dummy_var.alignment = 0;
    state.dbg_state = nullptr;

    // 7. LValue expression coverage (globals, dot_static, pm_field, indirect, errors)
    a_variable ext_gvar = {};
    ext_gvar.source_corresp.name = (char*)"ext_global_var";
    ext_gvar.type = &int_type;
    an_expr_node gvar_node = {};
    gvar_node.kind = enk_variable;
    gvar_node.type = &int_type;
    gvar_node.variant.variable.ptr = &ext_gvar;
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::ok); // existing global
    a_variable unnamed_var = {};
    gvar_node.variant.variable.ptr = &unnamed_var;
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    gvar_node.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    gvar_node.variant.variable.ptr = &ext_gvar;

    // Non-VLA subscript lvalue & rvalue
    vla_ptr_op.next = &vla_idx_op;
    vla_sub.type = &int_type;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_sub.type = &vol_int_ty;
    assert(llvm_lower_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;

    // dot_field, dot_static, pm_field, indirect lvalues & rvalues
    a_field norm_f = {};
    norm_f.offset = 4;
    norm_f.type = &int_type;
    an_expr_node f_node = {};
    f_node.kind = enk_field;
    f_node.variant.field.ptr = &norm_f;
    vla_ptr_op.next = &f_node;
    an_expr_node df_expr = {};
    df_expr.kind = enk_operation;
    df_expr.type = &int_type;
    df_expr.variant.operation.kind = eok_dot_field;
    df_expr.variant.operation.operands = &vla_ptr_op;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.type = &vol_int_ty;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    f_node.variant.field.ptr = &bf_f;
    df_expr.type = &int_type;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok); // bitfield load via dot_field
    vla_ptr_op.next = nullptr;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    vla_ptr_op.next = &gvar_node;
    df_expr.variant.operation.kind = eok_dot_static;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_lvalue_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;
    df_expr.variant.operation.kind = eok_indirect;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.variant.operation.kind = eok_add;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    assert(llvm_lower_lvalue_expression(&i32_arg1, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 8. Arithmetic expression full operator & type matrix
    assert(llvm_lower_lvalue_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_lvalue_expression(&gvar_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_arithmetic_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_logical_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_cast_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_call_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_expression(&gvar_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_compound_assignment(&gvar_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_compound_assignment(&gvar_node, &val) == llvm_gen_be_error_t::invalid_argument);

    ptr_type.variant.pointer.type = &int_type;
    an_expr_node arith_op1 = {}, arith_op2 = {};
    arith_op1.kind = enk_constant;
    arith_op2.kind = enk_constant;
    arith_op1.next = &arith_op2;
    an_expr_node arith_e = {};
    arith_e.kind = enk_operation;
    arith_e.variant.operation.operands = &arith_op1;

    for (a_type_ptr aty : {&int_type, &u32_ty, &float_type}) {
      arith_op1.type = aty;
      arith_op2.type = aty;
      arith_e.type = aty;
      for (an_expr_operator_kind k : {
             eok_negate, eok_unary_plus, eok_add, eok_subtract, eok_multiply,
             eok_divide, eok_remainder, eok_xconj}) {
        arith_e.variant.operation.kind = k;
        assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
      }
    }
    for (a_type_ptr ity : {&int_type, &u32_ty}) {
      arith_op1.type = ity;
      arith_op2.type = ity;
      arith_e.type = ity;
      for (an_expr_operator_kind k : {
             eok_complement, eok_and, eok_or, eok_xor, eok_shiftl, eok_shiftr}) {
        arith_e.variant.operation.kind = k;
        assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
      }
    }
    arith_op1.type = &float_type;
    arith_op2.type = &float_type;
    arith_e.type = &cplx_ty;
    for (an_expr_operator_kind k : {eok_fjadd, eok_jfadd, eok_fjsubtract, eok_jfsubtract}) {
      arith_e.variant.operation.kind = k;
      assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    }
    arith_e.type = &float_type;
    for (an_expr_operator_kind k : {eok_jmultiply, eok_jdivide}) {
      arith_e.variant.operation.kind = k;
      assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    }
    // Pointer arithmetic
    arith_op1.type = &ptr_type;
    arith_op2.type = &int_type;
    arith_e.type = &ptr_type;
    arith_e.variant.operation.kind = eok_padd;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.variant.operation.kind = eok_psubtract;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_op2.type = &ptr_type;
    arith_e.type = &i64_ty;
    arith_e.variant.operation.kind = eok_pdiff;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);

    // Non-pointer fallbacks in padd/psubtract/pdiff
    arith_op1.type = &int_type;
    arith_op2.type = &int_type;
    arith_e.variant.operation.kind = eok_padd;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.variant.operation.kind = eok_psubtract;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.variant.operation.kind = eok_pdiff;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    arith_e.variant.operation.kind = eok_assign;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 9. Logical & relational expression matrix
    a_type vec_ty = {}, fvec_ty = {}, bool_ty = {};
    state.type_cache[&vec_ty] = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
    state.type_cache[&fvec_ty] = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4);
    state.type_cache[&bool_ty] = llvm::Type::getInt1Ty(*state.context);

    an_expr_node log_e = {};
    log_e.kind = enk_operation;
    log_e.type = &int_type;
    log_e.variant.operation.operands = &arith_op1;
    for (an_expr_operator_kind k : {eok_land, eok_lor}) {
      log_e.variant.operation.kind = k;
      assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    }
    for (a_type_ptr lty : {&int_type, &u32_ty, &float_type, &ptr_type}) {
      arith_op1.type = lty;
      arith_op2.type = lty;
      for (an_expr_operator_kind k : {eok_eq, eok_ne, eok_lt, eok_le, eok_gt, eok_ge, eok_spaceship}) {
        log_e.variant.operation.kind = k;
        log_e.type = &int_type;
        assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
      }
    }
    // Vector comparison SExt
    arith_op1.type = &vec_ty;
    arith_op2.type = &vec_ty;
    log_e.type = &vec_ty;
    log_e.variant.operation.kind = eok_eq;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);

    // Default unsupported binary op in llvm_lower_logical_expression
    arith_op1.type = &int_type;
    arith_op2.type = &int_type;
    log_e.type = &int_type;
    log_e.variant.operation.kind = eok_add;
    assert(llvm_lower_logical_expression(&log_e, &val) == llvm_gen_be_error_t::unsupported_expr);

    // Unary eok_not (int, i1, vector)
    arith_op1.next = nullptr;
    log_e.variant.operation.kind = eok_not;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    log_e.type = &bool_ty;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    arith_op1.type = &vec_ty;
    log_e.type = &vec_ty;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_cmp_expr, &val) == llvm_gen_be_error_t::ok);
    arith_op1.next = &arith_op2;

    // 10. Cast expression matrix
    an_expr_node cast_e = {};
    cast_e.kind = enk_operation;
    cast_e.variant.operation.kind = eok_cast;
    cast_e.variant.operation.operands = &arith_op1;
    arith_op1.next = nullptr;
    std::pair<a_type_ptr, a_type_ptr> cast_pairs[] = {
      {&int_type, &int_type},
      {&ptr_type, &i64_ty},
      {&i64_ty, &ptr_type},
      {&float_type, &double_ty},
      {&double_ty, &float_type},
      {&float_type, &int_type},
      {&float_type, &u32_ty},
      {&int_type, &float_type},
      {&u32_ty, &float_type},
      {&int_type, &i64_ty},
      {&u32_ty, &i64_ty},
      {&i64_ty, &i8_ty},
      {&int_type, &cplx_double_ty},
      {&vec_ty, &fvec_ty},
    };
    for (const auto& cp : cast_pairs) {
      arith_op1.type = cp.first;
      cast_e.type = cp.second;
      assert(llvm_lower_expression(&cast_e, &val) == llvm_gen_be_error_t::ok);
    }
    cast_e.variant.operation.kind = eok_lvalue_cast;
    assert(llvm_lower_expression(&cast_e, &val) == llvm_gen_be_error_t::ok);

    // 11. Call expression: sret, byval, 2-reg aggregate split, virtual call, invoke, convertvector, shufflevector
    a_field big_f = {};
    big_f.type = &i64_ty;
    a_type big_struct_ty = {};
    big_struct_ty.kind = tk_struct;
    big_struct_ty.size = 24;
    big_struct_ty.variant.class_struct_union.field_list = &big_f;
    llvm::Type* big_llvm_ty = nullptr;
    assert(get_llvm_type(&big_struct_ty, &big_llvm_ty) == llvm_gen_be_error_t::ok);

    a_variable big_var = {};
    big_var.type = &big_struct_ty;
    state.local_vars[&big_var] = state.builder->CreateAlloca(big_llvm_ty);
    an_expr_node big_var_node = {};
    big_var_node.kind = enk_variable;
    big_var_node.type = &big_struct_ty;
    big_var_node.variant.variable.ptr = &big_var;

    a_variable ic_var2 = {};
    ic_var2.type = &ic_struct_ty;
    state.local_vars[&ic_var2] = state.builder->CreateAlloca(ic_llvm_ty);
    an_expr_node ic_var_node = {};
    ic_var_node.kind = enk_variable;
    ic_var_node.type = &ic_struct_ty;
    ic_var_node.variant.variable.ptr = &ic_var2;

    a_param_type p_big = {}, p_ic = {};
    p_big.type = &big_struct_ty;
    p_ic.type = &ic_struct_ty;
    p_big.next = &p_ic;
    a_routine_type_supplement sret_rts = {};
    sret_rts.param_type_list = &p_big;
    a_type sret_rout_ty = {};
    sret_rout_ty.kind = tk_routine;
    sret_rout_ty.variant.routine.return_type = &big_struct_ty;
    sret_rout_ty.variant.routine.extra_info = &sret_rts;

    a_routine sret_rout = {};
    sret_rout.source_corresp.name = (char*)"sret_byval_fn";
    sret_rout.type = &sret_rout_ty;
    an_expr_node sret_callee = {};
    sret_callee.kind = enk_routine;
    sret_callee.type = &sret_rout_ty;
    sret_callee.variant.routine.ptr = &sret_rout;
    sret_callee.next = &big_var_node;
    big_var_node.next = &ic_var_node;
    ic_var_node.next = nullptr;
    an_expr_node sret_call = {};
    sret_call.kind = enk_operation;
    sret_call.type = &big_struct_ty;
    sret_call.variant.operation.kind = eok_call;
    sret_call.variant.operation.operands = &sret_callee;
    assert(llvm_lower_call_expression(&sret_call, &val) == llvm_gen_be_error_t::ok);

    // 8-byte struct coerced to i64 on Win64 and unpacked on return
    a_param_type p_s8 = {};
    p_s8.type = &ic_struct_ty;
    a_routine_type_supplement s8_rts = {};
    s8_rts.param_type_list = &p_s8;
    a_type s8_rout_ty = {};
    s8_rout_ty.kind = tk_routine;
    s8_rout_ty.variant.routine.return_type = &ic_struct_ty;
    s8_rout_ty.variant.routine.extra_info = &s8_rts;
    a_routine s8_rout = {};
    s8_rout.source_corresp.name = (char*)"s8_win64_fn";
    s8_rout.type = &s8_rout_ty;
    an_expr_node s8_callee = {};
    s8_callee.kind = enk_routine;
    s8_callee.type = &s8_rout_ty;
    s8_callee.variant.routine.ptr = &s8_rout;
    s8_callee.next = &ic_var_node;
    ic_var_node.next = nullptr;
    an_expr_node s8_call = {};
    s8_call.kind = enk_operation;
    s8_call.type = &ic_struct_ty;
    s8_call.variant.operation.kind = eok_call;
    s8_call.variant.operation.operands = &s8_callee;
    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    assert(llvm_lower_call_expression(&s8_call, &val) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));

    // Virtual call on Itanium and MSVC, plus invoke with landing pad & attributes
    a_routine vcall_rout = {};
    vcall_rout.source_corresp.name = (char*)"vcall_test_fn";
    an_expr_node vcall_callee = {};
    vcall_callee.kind = enk_routine;
    vcall_callee.variant.routine.ptr = &vcall_rout;
    vcall_callee.next = &ptr_arg1;
    ptr_arg1.next = nullptr;
    an_expr_node vcall_expr = {};
    vcall_expr.kind = enk_operation;
    vcall_expr.type = &int_type;
    vcall_expr.variant.operation.kind = eok_call;
    vcall_expr.variant.operation.is_virtual_call = TRUE;
    vcall_expr.variant.operation.operands = &vcall_callee;
    assert(llvm_lower_call_expression(&vcall_expr, &val) == llvm_gen_be_error_t::ok);
    vcall_expr.variant.operation.kind = eok_dot_member_call;
    assert(llvm_lower_expression(&vcall_expr, &val) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    assert(llvm_lower_call_expression(&vcall_expr, &val) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));

    llvm::Function* fn = state.builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* dummy_lpad = llvm::BasicBlock::Create(*state.context, "lpad", fn);
    state.current_landing_pads.push_back(dummy_lpad);
    assert(llvm_lower_call_expression(&sret_call, &val) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.pop_back();

    // Target builtin with non-void callee_ty (hits overload_types.push_back(callee_ty->getReturnType()))
    a_routine tb_ov_rout = {};
    tb_ov_rout.source_corresp.name = (char*)"__builtin_sve_add_nxv4i32";
    tb_ov_rout.type = &rout_ty;
    an_expr_node tb_ov_callee = {};
    tb_ov_callee.kind = enk_routine;
    tb_ov_callee.type = &rout_ty;
    tb_ov_callee.variant.routine.ptr = &tb_ov_rout;
    tb_ov_callee.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node tb_ov_call = {};
    tb_ov_call.kind = enk_operation;
    tb_ov_call.type = &int_type;
    tb_ov_call.variant.operation.kind = eok_call;
    tb_ov_call.variant.operation.operands = &tb_ov_callee;
    assert(llvm_lower_call_expression(&tb_ov_call, &val) == llvm_gen_be_error_t::ok);

    // Indirect call without callee_ty
    an_expr_node ind_callee = ptr_arg1;
    ind_callee.type = &ptr_type;
    ind_callee.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node ind_call = {};
    ind_call.kind = enk_operation;
    ind_call.type = &int_type;
    ind_call.variant.operation.kind = eok_call;
    ind_call.variant.operation.operands = &ind_callee;
    assert(llvm_lower_call_expression(&ind_call, &val) == llvm_gen_be_error_t::ok);

    // Builtins: shufflevector, convertvector, trap, unreachable
    an_expr_node iv_arg1 = {}, iv_arg2 = {}, fv_arg1 = {};
    iv_arg1.kind = enk_constant;
    iv_arg1.type = &vec_ty;
    iv_arg2 = iv_arg1;
    fv_arg1.kind = enk_constant;
    fv_arg1.type = &fvec_ty;
    assert(call_test_builtin("__builtin_shufflevector", &vec_ty, {&iv_arg1, &iv_arg2, &n_ord[0], &dummy_op1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &vec_ty, {&iv_arg1, &iv_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &fvec_ty, {&fv_arg1, &fv_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &fvec_ty, {&iv_arg1, &fv_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &vec_ty, {&fv_arg1, &iv_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_trap", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_unreachable", &void_ty, {}) == llvm_gen_be_error_t::ok);
    llvm::BasicBlock* post_unreach_bb = llvm::BasicBlock::Create(*state.context, "post.unreach", fn);
    state.builder->SetInsertPoint(post_unreach_bb);

    // Vector subscript rvalue (CreateExtractElement)
    iv_arg1.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node vec_sub = {};
    vec_sub.kind = enk_operation;
    vec_sub.type = &int_type;
    vec_sub.variant.operation.kind = eok_subscript;
    vec_sub.variant.operation.operands = &iv_arg1;
    assert(llvm_lower_expression(&vec_sub, &val) == llvm_gen_be_error_t::ok);
    iv_arg1.next = nullptr;

    // 12. Remaining enk_* kinds in llvm_lower_expression
    assert(llvm_lower_expression(nullptr, &val) == llvm_gen_be_error_t::ok);
    // is_lvalue with non-lvalue op
    arith_op1.type = &int_type;
    arith_op2.type = &int_type;
    arith_op1.next = &arith_op2;
    arith_e.type = &int_type;
    arith_e.is_lvalue = TRUE;
    arith_e.variant.operation.kind = eok_add;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.is_lvalue = FALSE;

    // enk_braced_init_list: void, struct, and 1-element scalar
    braced_expr.type = &void_ty;
    assert(llvm_lower_expression(&braced_expr, &val) == llvm_gen_be_error_t::ok);
    braced_expr.type = &ic_struct_ty;
    assert(llvm_lower_expression(&braced_expr, &val) == llvm_gen_be_error_t::ok);
    braced_expr.type = &int_type;
    braced_expr.variant.braced_init_list = &i32_arg1;
    i32_arg1.next = nullptr;
    assert(llvm_lower_expression(&braced_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_reuse_value: cache hit, dik_zero, dik_constant, dik_constructor, dik_none
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok); // cache hit
    a_dynamic_init rv_zero = {}, rv_con = {}, rv_ctor = {}, rv_bad = {};
    rv_zero.kind = dik_zero;
    reuse_expr.variant.reused_value_init = &rv_zero;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_con.kind = dik_constant;
    rv_con.variant.constant.ptr = &c_ord[1];
    reuse_expr.variant.reused_value_init = &rv_con;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_ctor.kind = dik_constructor;
    reuse_expr.variant.reused_value_init = &rv_ctor;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_bad.kind = dik_none;
    reuse_expr.variant.reused_value_init = &rv_bad;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // enk_c11_generic, enk_fold, enk_pack_index, enk_routine (null)
    an_expr_node gen_e = {};
    gen_e.kind = enk_c11_generic;
    gen_e.type = &int_type;
    assert(llvm_lower_expression(&gen_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    gen_e.variant.c11_generic.result = &i32_arg1;
    assert(llvm_lower_expression(&gen_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node fold_e = {};
    fold_e.kind = enk_fold;
    fold_e.type = &int_type;
    assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    fold_e.variant.fold.operands = &i32_arg1;
    for (a_token_kind tk : {tok_plus, tok_minus, tok_star, tok_ampersand, tok_or, tok_excl_or, tok_comma, tok_assign}) {
      fold_e.variant.fold.operator_token = tk;
      assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::ok);
    }
    flt_arg1.next = &flt_arg2;
    flt_arg2.next = nullptr;
    fold_e.type = &float_type;
    fold_e.variant.fold.operands = &flt_arg1;
    for (a_token_kind tk : {tok_plus, tok_minus, tok_star}) {
      fold_e.variant.fold.operator_token = tk;
      assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::ok);
    }
    i32_arg1.next = nullptr;
    flt_arg1.next = nullptr;

    an_expr_node pack_e = {};
    pack_e.kind = enk_pack_index;
    pack_e.type = &int_type;
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    i32_arg1.next = &i32_arg2;
    pack_e.variant.pack_index.expr = &i32_arg1;
    pack_e.variant.pack_index.index_expr = &n_ord[1];
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    an_expr_node null_rout_e = {};
    null_rout_e.kind = enk_routine;
    null_rout_e.type = &ptr_type;
    assert(llvm_lower_expression(&null_rout_e, &val) == llvm_gen_be_error_t::ok);

    // eok_question (int & void), eok_assign (bitfield & >16B struct memcpy & store & null rhs), eok_dynamic_cast, eok_pm_base_class_cast, eok_dot_pm_call
    an_expr_node q_e = {};
    q_e.kind = enk_operation;
    q_e.type = &int_type;
    q_e.variant.operation.kind = eok_question;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = &i32_arg3;
    i32_arg3.next = nullptr;
    q_e.variant.operation.operands = &i32_arg1;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    q_e.type = &void_ty;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    // eok_question with terminated true arm and terminated false arm
    an_expr_node q_throw_arm = {};
    q_throw_arm.kind = enk_throw;
    q_throw_arm.type = &void_ty;
    q_e.type = &int_type;
    i32_arg1.next = &q_throw_arm;
    q_throw_arm.next = &i32_arg3;
    i32_arg3.next = nullptr;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = &q_throw_arm;
    q_throw_arm.next = nullptr;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;
    i32_arg2.next = nullptr;

    // Bitfield assign, >16B struct assign, and unsupported assign (null rhs)
    df_expr.is_lvalue = TRUE;
    df_expr.variant.operation.kind = eok_dot_field;
    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &bf_f;
    df_expr.next = &i32_arg1;
    assign_expr.variant.operation.operands = &df_expr;
    assign_expr.position.seq = 0;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.next = nullptr;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    vla_ptr_op.next = nullptr;

    an_expr_node big_lhs = big_var_node;
    big_lhs.is_lvalue = TRUE;
    an_expr_node big_rhs_load = big_var_node;
    big_rhs_load.is_lvalue = FALSE;
    big_lhs.next = &big_rhs_load;
    assign_expr.type = &big_struct_ty;
    assign_expr.variant.operation.operands = &big_lhs;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok); // MemCpy path
    an_expr_node big_rhs_const = {};
    big_rhs_const.kind = enk_constant;
    big_rhs_const.type = &big_struct_ty;
    big_lhs.next = &big_rhs_const;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok); // Store path

    pm_op1.next = &pm_op2;
    assert(llvm_lower_expression(&pm_cast_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_call_expr, &val) == llvm_gen_be_error_t::ok);
    an_expr_node dcast_e = {};
    dcast_e.kind = enk_operation;
    dcast_e.type = &ptr_type;
    dcast_e.variant.operation.kind = eok_dynamic_cast;
    dcast_e.variant.operation.operands = &ptr_arg1;
    assert(llvm_lower_expression(&dcast_e, &val) == llvm_gen_be_error_t::ok);

    // enk_await, enk_builtin_operation (offsetof, bit_cast, has_attribute), enk_statement, enk_typeid
    an_expr_node await_e = {};
    await_e.kind = enk_await;
    await_e.type = &int_type;
    assert(llvm_lower_expression(&await_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node bok_e = {};
    bok_e.kind = enk_builtin_operation;
    bok_e.type = &i64_ty;
    bok_e.variant.builtin_operation.kind = bok_offsetof;
    an_expr_node bok_op1 = i32_arg1;
    f_node.variant.field.ptr = &norm_f;
    an_expr_node bok_df = df_expr;
    bok_df.is_lvalue = FALSE;
    vla_ptr_op.next = &f_node;
    bok_op1.next = &f_node;
    f_node.next = &bok_df;
    bok_df.next = nullptr;
    bok_e.variant.builtin_operation.operands = &bok_op1;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    f_node.next = nullptr;

    bok_e.variant.builtin_operation.kind = bok_builtin_bit_cast;
    bok_e.type = &float_type;
    bok_op1.next = &i32_arg1;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    bok_e.type = &int_type;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    bok_e.type = &ic_struct_ty;
    bok_op1.next = &ic_var_node;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);

    // bok_builtin_has_attribute
    an_attribute attr1 = {};
    attr1.kind = ak_align;
    ext_gvar.source_corresp.attributes = &attr1;
    ext_fn_rout.source_corresp.attributes = &attr1;
    norm_f.source_corresp.attributes = &attr1;
    int_type.source_corresp.attributes = &attr1;
    c_ord[0].source_corresp.attributes = &attr1;

    an_expr_node paren_subj = {};
    paren_subj.kind = enk_operation;
    paren_subj.variant.operation.kind = eok_parens;
    paren_subj.variant.operation.operands = &gvar_node;
    paren_subj.next = &n_ord[0];
    bok_e.variant.builtin_operation.kind = bok_builtin_has_attribute;
    bok_e.variant.builtin_operation.operands = &paren_subj;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);

    df_expr.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &df_expr;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    fn_callee_op.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &fn_callee_op;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    an_expr_node type_op_subj = {};
    type_op_subj.kind = enk_type_operand;
    type_op_subj.variant.type_operand.type = &int_type;
    type_op_subj.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &type_op_subj;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &arith_e;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.next = nullptr;
    fn_callee_op.next = &fn_arg1;
    df_expr.next = nullptr;
    vla_ptr_op.next = nullptr;

    bok_e.variant.builtin_operation.kind = static_cast<a_builtin_operation_kind>(250);
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::unsupported_expr);

    // enk_statement, enk_typeid, enk_vla_dealloc
    a_statement stmt_ok = {}, stmt_err = {};
    stmt_ok.kind = stmk_expr;
    stmt_err.kind = stmk_return;
    an_expr_node stmt_e = {};
    stmt_e.kind = enk_statement;
    stmt_e.type = &int_type;
    stmt_e.variant.statement = &stmt_ok;
    assert(llvm_lower_expression(&stmt_e, &val) == llvm_gen_be_error_t::ok);
    stmt_e.type = &void_ty;
    assert(llvm_lower_expression(&stmt_e, &val) == llvm_gen_be_error_t::ok);
    stmt_e.variant.statement = &stmt_err;
    assert(llvm_lower_expression(&stmt_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node tid_e = {};
    tid_e.kind = enk_typeid;
    tid_e.type = &int_type;
    type_op_subj.next = nullptr;
    tid_e.variant.typeid_info.type_with_opt_expr = &type_op_subj;
    assert(llvm_lower_expression(&tid_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node vla_dealloc_e = {};
    vla_dealloc_e.kind = enk_vla_dealloc;
    vla_dealloc_e.variant.vla_variable = &dummy_var;
    state.vla_saved_stacks[&dummy_var] = bf_ptr;
    assert(llvm_lower_expression(&vla_dealloc_e, &val) == llvm_gen_be_error_t::ok);

    // 13. enk_throw (rethrow & throw, Itanium & MSVC, with/without landing pad & destructor)
    an_expr_node throw_e = {};
    throw_e.kind = enk_throw;
    throw_e.type = &void_ty;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium rethrow
    state.current_landing_pads.push_back(dummy_lpad);
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium rethrow with invoke
    state.current_landing_pads.pop_back();
    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // MSVC rethrow

    a_routine exc_dtor = {};
    exc_dtor.source_corresp.name = (char*)"exc_dtor_fn";
    exc_dtor.type = &rout_ty;
    a_dynamic_init exc_dip = {};
    exc_dip.kind = dik_zero;
    exc_dip.destructor = &exc_dtor;
    a_throw_supplement t_supp = {};
    t_supp.type = &int_type;
    t_supp.dynamic_init = &exc_dip;
    throw_e.variant.throw_info = &t_supp;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // MSVC throw
    state.current_landing_pads.push_back(dummy_lpad);
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // MSVC throw with invoke & cached _TI

    // MSVC throw of derived class with base classes (null type, named/unnamed, size 0/8)
    a_type base_cls1 = {}, base_cls2 = {}, derived_exc_cls = {};
    base_cls1.kind = tk_struct; base_cls1.size = 8; base_cls1.source_corresp.name = (char*)"BaseExc1";
    base_cls2.kind = tk_class; base_cls2.size = 0; base_cls2.source_corresp.name = nullptr;
    a_base_class bc_null = {}, bc_dir1 = {}, bc_dir2 = {};
    bc_null.type = nullptr; bc_null.next = &bc_dir1;
    bc_dir1.type = &base_cls1; bc_dir1.offset = 0; bc_dir1.next = &bc_dir2;
    bc_dir2.type = &base_cls2; bc_dir2.offset = 8; bc_dir2.next = nullptr;
    a_class_type_supplement derived_exc_supp = {};
    derived_exc_supp.base_classes = &bc_null;
    derived_exc_cls.kind = tk_class;
    derived_exc_cls.size = 16;
    derived_exc_cls.source_corresp.name = (char*)"DerivedExc";
    derived_exc_cls.variant.class_struct_union.extra_info = &derived_exc_supp;
    t_supp.type = &derived_exc_cls;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    a_type struct_no_supp_exc = {};
    struct_no_supp_exc.kind = tk_struct;
    struct_no_supp_exc.source_corresp.name = (char*)"StructNoSuppExc";
    t_supp.type = &struct_no_supp_exc;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    t_supp.type = &int_type;
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    exc_dtor.source_corresp.name = (char*)"itanium_exc_dtor_fn";
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium throw with invoke & new dtor
    state.current_landing_pads.pop_back();
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium throw with call & existing dtor

    // 14. enk_temp_init (has_temporary_lifetime), enk_object_lifetime (destructions & cleanups), enk_new_delete (cookies & exception cleanup)
    temp_dtor_dip.has_temporary_lifetime = TRUE;
    a_dynamic_init dtor_list_item = {};
    dtor_list_item.destructor = &temp_dtor_routine;
    dtor_list_item.variable = &dummy_var;
    obj_lifetime.destructions = &dtor_list_item;
    obj_lifetime_expr.variant.object_lifetime.expr = &temp_dtor_expr;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) == llvm_gen_be_error_t::ok);

    // Array new[] and delete[] with nontrivial destructor cookie (size == 777)
    a_type cookie_ty = int_type;
    cookie_ty.size = 777;
    nd_array.type = &cookie_ty;
    assert(llvm_lower_expression(&new_array_expr, &val) == llvm_gen_be_error_t::ok);
    delete_array_nd.type = &cookie_ty;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::ok);
    a_dynamic_init del_dtor_dip = {};
    del_dtor_dip.destructor = &temp_dtor_routine;
    delete_array_nd.dynamic_init = &del_dtor_dip;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::ok);
    del_dtor_dip.destructor = nullptr;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::ok);
    del_dtor_dip.destructor = &temp_dtor_routine;
    a_type bad_cookie_ty = bad_ty;
    bad_cookie_ty.size = 777;
    delete_array_nd.type = &bad_cookie_ty;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::unsupported_type);
    delete_array_nd.type = &cookie_ty;
    delete_array_nd.dynamic_init = nullptr;

    // Single-object delete with dynamic_init (destructor != nullptr and destructor == nullptr)
    delete_nd.dynamic_init = &del_dtor_dip;
    assert(llvm_lower_expression(&delete_expr, &val) == llvm_gen_be_error_t::ok);
    del_dtor_dip.destructor = nullptr;
    assert(llvm_lower_expression(&delete_expr, &val) == llvm_gen_be_error_t::ok);
    delete_nd.dynamic_init = nullptr;

    // new with dynamic_init and freeing_of_storage_on_exception (new cleanup function)
    a_routine custom_del_routine = {};
    custom_del_routine.source_corresp.name = (char*)"custom_op_delete";
    custom_del_routine.type = &int_type;
    a_dynamic_init new_free_dip = {};
    new_free_dip.destructor = &custom_del_routine;
    nd.dynamic_init = &exc_dip;
    nd.freeing_of_storage_on_exception = &new_free_dip;
    assert(llvm_lower_expression(&new_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&new_expr, &val) == llvm_gen_be_error_t::ok); // second call when parent_fn->hasPersonalityFn() is true
    nd.dynamic_init = nullptr;
    nd.freeing_of_storage_on_exception = nullptr;

    // 15. llvm_lower_compound_assignment full operator & type matrix
    an_expr_node ca_lhs = dummy_op1;
    ca_lhs.is_lvalue = TRUE;
    an_expr_node ca_rhs = i32_arg1;
    ca_lhs.next = &ca_rhs;
    an_expr_node ca_expr = {};
    ca_expr.kind = enk_operation;
    ca_expr.type = &int_type;
    ca_expr.variant.operation.operands = &ca_lhs;

    for (a_type_ptr cty : {&int_type, &u32_ty, &float_type}) {
      ca_lhs.type = cty;
      ca_rhs.type = cty;
      ca_expr.type = cty;
      for (an_expr_operator_kind k : {
             eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr,
             eok_add_assign, eok_subtract_assign, eok_multiply_assign,
             eok_divide_assign, eok_remainder_assign}) {
        ca_lhs.next = (k == eok_pre_incr || k == eok_post_incr || k == eok_pre_decr || k == eok_post_decr) ? nullptr : &ca_rhs;
        ca_expr.variant.operation.kind = k;
        assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }
    for (a_type_ptr ity : {&int_type, &u32_ty}) {
      ca_lhs.type = ity;
      ca_rhs.type = ity;
      ca_expr.type = ity;
      ca_lhs.next = &ca_rhs;
      for (an_expr_operator_kind k : {
             eok_and_assign, eok_or_assign, eok_xor_assign,
             eok_shiftl_assign, eok_shiftr_assign}) {
        ca_expr.variant.operation.kind = k;
        assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }
    // padd_assign, psubtract_assign, bassign
    ca_lhs = va_var_node;
    ca_lhs.is_lvalue = TRUE;
    ca_lhs.type = &ptr_type;
    ca_rhs.type = &int_type;
    ca_lhs.next = &ca_rhs;
    ca_expr.type = &ptr_type;
    ca_expr.variant.operation.kind = eok_padd_assign;
    assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_expr.variant.operation.kind = eok_psubtract_assign;
    assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_rhs = va_var_node;
    ca_lhs.next = &ca_rhs;
    ca_expr.variant.operation.kind = eok_bassign;
    assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);

    // Bitfield & volatile compound assignment + unsupported op
    df_expr.is_lvalue = TRUE;
    df_expr.type = &int_type;
    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &bf_f;
    df_expr.next = nullptr;
    ca_expr.variant.operation.operands = &df_expr;
    ca_expr.variant.operation.kind = eok_pre_incr;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_lhs = dummy_op1;
    ca_lhs.type = &vol_int_ty;
    ca_lhs.next = nullptr;
    ca_expr.variant.operation.operands = &ca_lhs;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_expr.variant.operation.kind = eok_eq;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 16. Remaining switch arms, edge cases, and error percolation
    a_constant bad_con = {};
    bad_con.source_corresp.decl_position.seq = 888; // evaluate_constant returns unsupported_expr
    an_expr_node bad_op = {};
    bad_op.kind = enk_constant;
    bad_op.type = &int_type;
    bad_op.variant.constant.ptr = &bad_con;

    // Null out_val checks
    assert(llvm_lower_arithmetic_expression(&arith_e, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_logical_expression(&log_e, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_cast_expression(&cast_e, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_call_expression(&abi_call_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);

    // lower_binary_operands & load_from_lvalue_ptr error percolation
    bad_op.next = &i32_arg1;
    arith_e.variant.operation.operands = &bad_op;
    arith_e.variant.operation.kind = eok_add;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &bad_op;
    bad_op.next = nullptr;
    arith_e.variant.operation.operands = &i32_arg1;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // load_from_lvalue_ptr error (bad_ty on enk_variable rvalue)
    an_expr_node bad_var_rval = dummy_op1;
    bad_var_rval.is_lvalue = FALSE;
    bad_var_rval.type = &bad_ty;
    assert(llvm_lower_expression(&bad_var_rval, &val) != llvm_gen_be_error_t::ok);

    // Remaining switch cases in llvm_lower_lvalue_expression & llvm_lower_expression
    pm_field_expr.variant.operation.kind = eok_pm_points_to_field;
    assert(llvm_lower_lvalue_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    pm_cast_expr.variant.operation.kind = eok_pm_derived_class_cast;
    assert(llvm_lower_expression(&pm_cast_expr, &val) == llvm_gen_be_error_t::ok);
    dcast_e.variant.operation.kind = eok_ref_dynamic_cast;
    assert(llvm_lower_expression(&dcast_e, &val) == llvm_gen_be_error_t::ok);
    pm_call_expr.variant.operation.kind = eok_points_to_pm_call;
    assert(llvm_lower_expression(&pm_call_expr, &val) == llvm_gen_be_error_t::ok);

    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &norm_f;
    df_expr.variant.operation.kind = eok_points_to_field;
    df_expr.type = &int_type;
    df_expr.is_lvalue = TRUE;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.is_lvalue = FALSE;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = &gvar_node;
    df_expr.variant.operation.kind = eok_points_to_static;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);

    // is_lvalue = TRUE with eok_subscript and eok_indirect
    vla_ptr_op.next = &vla_idx_op;
    vla_sub.is_lvalue = TRUE;
    assert(llvm_lower_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_sub.is_lvalue = FALSE;
    vla_ptr_op.next = nullptr;
    df_expr.variant.operation.kind = eok_indirect;
    df_expr.is_lvalue = TRUE;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.is_lvalue = FALSE;

    // CLI unsupported operators
    for (an_expr_operator_kind ck : {eok_handle_to, eok_handle_to_box, eok_box, eok_unbox, eok_unbox_lvalue, eok_cli_subscript}) {
      df_expr.variant.operation.kind = ck;
      assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    }

    // enk_yield
    await_e.kind = enk_yield;
    assert(llvm_lower_expression(&await_e, &val) == llvm_gen_be_error_t::ok);

    // enk_fold with tok_and_and and tok_or_or
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    fold_e.type = &int_type;
    fold_e.variant.fold.operands = &i32_arg1;
    for (a_token_kind tk : {tok_and_and, tok_or_or}) {
      fold_e.variant.fold.operator_token = tk;
      assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::ok);
    }
    i32_arg1.next = nullptr;

    // enk_reuse_value with nullptr, dik_class_result_via_ctor, dik_nonconstant_aggregate, dik_bitwise_copy
    reuse_expr.variant.reused_value_init = nullptr;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::invalid_argument);
    a_dynamic_init rv_via_ctor = {}, rv_agg = {}, rv_bcopy = {};
    rv_via_ctor.kind = dik_class_result_via_ctor;
    rv_via_ctor.variant.expression = &i32_arg1;
    reuse_expr.variant.reused_value_init = &rv_via_ctor;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_agg.kind = dik_nonconstant_aggregate;
    reuse_expr.variant.reused_value_init = &rv_agg;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_bcopy.kind = dik_bitwise_copy;
    reuse_expr.variant.reused_value_init = &rv_bcopy;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_pack_index with null index_expr and non-constant index_expr
    i32_arg1.next = &i32_arg2;
    pack_e.variant.pack_index.expr = &i32_arg1;
    pack_e.variant.pack_index.index_expr = nullptr;
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    pack_e.variant.pack_index.index_expr = &dummy_op1;
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    pack_e.variant.pack_index.index_expr = &n_ord[5]; // clamps to last element
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // eok_question with i1 condition
    an_expr_node bool_cond = {};
    bool_cond.kind = enk_constant;
    bool_cond.type = &bool_ty;
    bool_cond.next = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    q_e.type = &int_type;
    q_e.variant.operation.operands = &bool_cond;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // Target builtin cached lookup + non-constant atomic ordering + 32-bit __atomic_is_lock_free
    assert(call_test_builtin("__builtin_ia32_pause", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_ia32_pause", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_load_n", &int_type, {&ptr_arg1, &dummy_op1}) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("i686-unknown-linux-gnu"));
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));

    // bok_builtin_operation null operands & bok_builtin_has_attribute with null arg2 & unmatched attr
    bok_e.variant.builtin_operation.operands = nullptr;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::invalid_argument);
    bok_e.variant.builtin_operation.kind = bok_builtin_has_attribute;
    gvar_node.next = nullptr;
    bok_e.variant.builtin_operation.operands = &gvar_node;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    an_attribute attr2 = {};
    attr2.kind = ak_noreturn;
    c_ord[1].source_corresp.attributes = &attr2;
    gvar_node.next = &n_ord[1];
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    gvar_node.next = nullptr;

    // enk_throw with type_size == 0, dynamic_init == nullptr, named type, and null dtor
    a_type zero_sz_ty = int_type;
    zero_sz_ty.size = 0;
    zero_sz_ty.source_corresp.name = (char*)"EmptyExc";
    t_supp.type = &zero_sz_ty;
    t_supp.dynamic_init = nullptr;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    exc_dip.destructor = nullptr;
    t_supp.dynamic_init = &exc_dip;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);

    // enk_vla_dealloc with unmapped variable, enk_temp_init with null dynamic_init, enk_object_lifetime with null inner_expr & reused_values
    vla_dealloc_e.variant.vla_variable = &unnamed_var;
    assert(llvm_lower_expression(&vla_dealloc_e, &val) == llvm_gen_be_error_t::ok);
    an_expr_node empty_temp_e = {};
    empty_temp_e.kind = enk_temp_init;
    empty_temp_e.type = &int_type;
    assert(llvm_lower_expression(&empty_temp_e, &val) == llvm_gen_be_error_t::ok);

    state.reused_values[&dtor_list_item] = bf_ptr;
    obj_lifetime_expr.variant.object_lifetime.expr = nullptr;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) == llvm_gen_be_error_t::ok);
    dtor_list_item.destructor = nullptr;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_new_delete edge cases (null nd, null routine, unnamed routine, placement_new custom, dynamic_init without exception cleanup)
    an_expr_node bad_nd_e = {};
    bad_nd_e.kind = enk_new_delete;
    bad_nd_e.type = &ptr_type;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::invalid_argument);
    a_new_delete_supplement custom_nd = {};
    custom_nd.is_new = TRUE;
    custom_nd.type = &int_type;
    bad_nd_e.variant.new_delete = &custom_nd;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    a_routine custom_new_rt = {};
    custom_nd.routine = &custom_new_rt;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    custom_new_rt.source_corresp.name = (char*)"my_placement_new";
    custom_nd.placement_new = TRUE;
    custom_nd.arg = &ptr_arg1;
    ptr_arg1.next = nullptr;
    custom_nd.dynamic_init = &exc_dip;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::ok);

    // Additional branch and error percolation tests
    // 1. enk_routine with FunctionType
    a_routine fn_ty_rt = {};
    fn_ty_rt.source_corresp.name = (char*)"fn_with_rout_ty";
    fn_ty_rt.type = &rout_ty;
    an_expr_node fn_ty_rt_e = {};
    fn_ty_rt_e.kind = enk_routine;
    fn_ty_rt_e.type = &ptr_type;
    fn_ty_rt_e.variant.routine.ptr = &fn_ty_rt;
    assert(llvm_lower_expression(&fn_ty_rt_e, &val) == llvm_gen_be_error_t::ok);

    // 2. eok_assign with dbg_state != nullptr and op1 != enk_variable
    state.dbg_state = &dbg_st;
    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &norm_f;
    df_expr.variant.operation.kind = eok_dot_field;
    df_expr.type = &int_type;
    df_expr.is_lvalue = TRUE;
    df_expr.next = &i32_arg1;
    i32_arg1.next = nullptr;
    assign_expr.type = &int_type;
    assign_expr.variant.operation.operands = &df_expr;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    state.dbg_state = nullptr;

    // 3. bok_builtin_bit_cast with scalar src and aggregate dest
    bok_op1.next = &i32_arg1;
    bok_e.variant.builtin_operation.kind = bok_builtin_bit_cast;
    bok_e.variant.builtin_operation.operands = &bok_op1;
    bok_e.type = &ic_struct_ty;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);

    // 4. enk_new_delete when parent_fn->hasPersonalityFn() is already true
    new_expr.variant.new_delete->is_new = TRUE;
    new_expr.variant.new_delete->arg = nullptr;
    assert(llvm_lower_expression(&new_expr, &val) == llvm_gen_be_error_t::ok);

    // 5. llvm_lower_lvalue_expression error percolation
    a_variable bad_gvar = {};
    bad_gvar.source_corresp.name = (char*)"bad_gvar_ty";
    an_expr_node bad_gvar_e = {};
    bad_gvar_e.kind = enk_variable;
    bad_gvar_e.type = &bad_ty;
    bad_gvar_e.variant.variable.ptr = &bad_gvar;
    assert(llvm_lower_lvalue_expression(&bad_gvar_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node bad_lval = {};
    bad_lval.kind = enk_variable;
    bad_lval.type = &int_type;
    an_expr_node real_e = {};
    real_e.kind = enk_operation;
    real_e.type = &int_type;
    real_e.variant.operation.kind = eok_real_part;
    real_e.variant.operation.operands = &bad_lval;
    assert(llvm_lower_lvalue_expression(&real_e, &val) != llvm_gen_be_error_t::ok);
    an_expr_node local_bad_ty_var = dummy_op1;
    local_bad_ty_var.type = &bad_ty;
    real_e.variant.operation.operands = &local_bad_ty_var;
    assert(llvm_lower_lvalue_expression(&real_e, &val) != llvm_gen_be_error_t::ok);

    bad_op.next = &gvar_node;
    df_expr.variant.operation.kind = eok_dot_static;
    df_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) != llvm_gen_be_error_t::ok);
    df_expr.variant.operation.kind = eok_indirect;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) != llvm_gen_be_error_t::ok);

    vla_ptr_op.next = &bad_op;
    bad_op.next = nullptr;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    vla_ptr_op.next = &vla_idx_op;
    vla_sub.type = &bad_ty;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    vla_sub.type = &vla_outer_arr;
    vla_inner_arr.variant.array.element_type = &bad_ty;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    vla_inner_arr.variant.array.element_type = &int_type;

    // 6. llvm_lower_arithmetic_expression & llvm_lower_logical_expression & llvm_lower_cast_expression error percolation
    a_type ptr_to_bad_ty = {};
    ptr_to_bad_ty.kind = tk_pointer;
    ptr_to_bad_ty.size = 8;
    ptr_to_bad_ty.variant.pointer.type = &bad_ty;
    ptr_arg1.type = &ptr_to_bad_ty;
    ptr_arg1.next = &i32_arg1;
    arith_e.variant.operation.kind = eok_padd;
    arith_e.variant.operation.operands = &ptr_arg1;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.type = &ptr_type;
    ptr_arg1.next = nullptr;

    log_e.type = &bad_ty;
    log_e.variant.operation.kind = eok_eq;
    log_e.variant.operation.operands = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    log_e.type = &int_type;
    log_e.variant.operation.kind = eok_land;
    bad_op.next = &i32_arg2;
    log_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &bad_op;
    bad_op.next = nullptr;
    log_e.variant.operation.operands = &i32_arg1;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    log_e.variant.operation.kind = eok_eq;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    cast_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_cast_expression(&cast_e, &val) != llvm_gen_be_error_t::ok);
    cast_e.variant.operation.operands = &i32_arg1;
    cast_e.type = &bad_ty;
    assert(llvm_lower_cast_expression(&cast_e, &val) != llvm_gen_be_error_t::ok);

    // 7. llvm_lower_call_expression error percolation
    a_type bad_rout_ty = {};
    bad_rout_ty.kind = tk_routine;
    bad_rout_ty.variant.routine.return_type = &bad_ty;
    fn_callee_op.type = &bad_rout_ty;
    assert(llvm_lower_call_expression(&abi_call_expr, &val) != llvm_gen_be_error_t::ok);
    fn_callee_op.type = &ptr_rout_ty;
    fn_callee_op.next = &bad_op;
    bad_op.next = nullptr;
    assert(llvm_lower_call_expression(&abi_call_expr, &val) != llvm_gen_be_error_t::ok);
    fn_callee_op.next = &fn_arg1;

    vcall_callee.next = nullptr;
    assert(llvm_lower_call_expression(&vcall_expr, &val) != llvm_gen_be_error_t::ok);
    vcall_callee.next = &ptr_arg1;
    vcall_expr.type = &bad_ty;
    assert(llvm_lower_call_expression(&vcall_expr, &val) != llvm_gen_be_error_t::ok);
    vcall_expr.type = &int_type;

    ind_call.variant.operation.operands = &bad_op;
    assert(llvm_lower_call_expression(&ind_call, &val) != llvm_gen_be_error_t::ok);
    a_type ref_bad_rout_ty = {};
    ref_bad_rout_ty.kind = tk_pointer;
    ref_bad_rout_ty.size = 555; // is_reference_type == TRUE -> skipped by top unwrap, caught by get_llvm_function_type
    ref_bad_rout_ty.variant.pointer.type = &bad_rout_ty;
    ind_callee.type = &ref_bad_rout_ty;
    ind_callee.next = nullptr;
    ind_call.variant.operation.operands = &ind_callee;
    assert(llvm_lower_call_expression(&ind_call, &val) != llvm_gen_be_error_t::ok);
    ind_callee.type = &ptr_type;
    ind_call.type = &bad_ty;
    assert(llvm_lower_call_expression(&ind_call, &val) != llvm_gen_be_error_t::ok);
    ind_call.type = &int_type;

    // Direct call with op1->type = nullptr (uses f->getFunctionType()) and expr->type = &bad_ty (hits line 1551)
    an_expr_node direct_no_ty_callee = fn_ty_rt_e;
    direct_no_ty_callee.type = nullptr;
    direct_no_ty_callee.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node direct_bad_ret_call = {};
    direct_bad_ret_call.kind = enk_operation;
    direct_bad_ret_call.type = &bad_ty;
    direct_bad_ret_call.variant.operation.kind = eok_call;
    direct_bad_ret_call.variant.operation.operands = &direct_no_ty_callee;
    assert(llvm_lower_call_expression(&direct_bad_ret_call, &val) != llvm_gen_be_error_t::ok);

    a_routine b_rt = {};
    an_expr_node b_callee = {};
    b_callee.kind = enk_routine;
    b_callee.variant.routine.ptr = &b_rt;
    an_expr_node b_call = {};
    b_call.kind = enk_operation;
    b_call.variant.operation.kind = eok_call;
    b_call.variant.operation.operands = &b_callee;
    an_expr_node bad_ptr_arg = ptr_arg1;
    bad_ptr_arg.type = &ptr_to_bad_ty;
    b_rt.source_corresp.name = (char*)"__builtin_convertvector";
    b_call.type = &bad_ty;
    b_callee.next = &iv_arg1;
    iv_arg1.next = nullptr;
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_load_n";
    b_callee.next = &ptr_arg1;
    ptr_arg1.next = &n_ord[0];
    n_ord[0].next = nullptr;
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_load";
    b_call.type = &void_ty;
    ptr_arg1.next = &bad_ptr_arg;
    bad_ptr_arg.next = &n_ord[0];
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_store";
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_exchange";
    bad_ptr_arg.next = &ptr_arg3;
    ptr_arg3.next = &n_ord[0];
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.next = nullptr;
    bad_ptr_arg.next = nullptr;
    ptr_arg3.next = nullptr;

    // 8. llvm_lower_expression error percolation
    braced_expr.type = &bad_ty;
    assert(llvm_lower_expression(&braced_expr, &val) != llvm_gen_be_error_t::ok);
    braced_expr.type = &ic_struct_ty;
    braced_expr.variant.braced_init_list = &bad_op;
    assert(llvm_lower_expression(&braced_expr, &val) != llvm_gen_be_error_t::ok);

    a_dynamic_init rv_err1 = {}, rv_err2 = {}, rv_err3 = {}, rv_err4 = {};
    rv_err1.kind = dik_expression;
    rv_err1.variant.expression = &bad_op;
    reuse_expr.type = &int_type;
    reuse_expr.variant.reused_value_init = &rv_err1;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);
    rv_err2.kind = dik_zero;
    reuse_expr.type = &bad_ty;
    reuse_expr.variant.reused_value_init = &rv_err2;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);
    reuse_expr.type = &int_type;
    rv_err3.kind = dik_constant;
    rv_err3.variant.constant.ptr = &bad_con;
    reuse_expr.variant.reused_value_init = &rv_err3;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);
    a_dynamic_init bad_dip = {};
    bad_dip.kind = dik_none;
    rv_err4.kind = dik_bitwise_copy;
    rv_err4.variable = &bad_gvar;
    reuse_expr.variant.reused_value_init = &rv_err4;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);

    fold_e.variant.fold.operands = &bad_op;
    assert(llvm_lower_expression(&fold_e, &val) != llvm_gen_be_error_t::ok);
    pack_e.variant.pack_index.expr = &i32_arg1;
    pack_e.variant.pack_index.index_expr = &bad_op;
    assert(llvm_lower_expression(&pack_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node bad_con_ty_e = {};
    bad_con_ty_e.kind = enk_constant;
    bad_con_ty_e.type = &bad_ty;
    assert(llvm_lower_expression(&bad_con_ty_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node bad_rout_e = {};
    bad_rout_e.kind = enk_routine;
    bad_rout_e.type = &bad_ty;
    assert(llvm_lower_expression(&bad_rout_e, &val) != llvm_gen_be_error_t::ok);
    a_routine err_rt = {};
    err_rt.source_corresp.name = (char*)"err_rt_fn";
    err_rt.type = &bad_ty;
    bad_rout_e.type = &ptr_type;
    bad_rout_e.variant.routine.ptr = &err_rt;
    assert(llvm_lower_expression(&bad_rout_e, &val) != llvm_gen_be_error_t::ok);

    assert(llvm_lower_expression(&bad_lval, &val) != llvm_gen_be_error_t::ok);

    // eok_subscript, eok_dot_field, eok_dot_vacuous_destructor_call, eok_real_part rvalue errors
    bad_op.next = &i32_arg1;
    vla_sub.type = &int_type;
    vla_sub.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &i32_arg2;
    vla_sub.variant.operation.operands = &i32_arg1; // non-pointer -> lvalue expression fails
    assert(llvm_lower_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);

    df_expr.is_lvalue = FALSE;
    df_expr.variant.operation.kind = eok_dot_field;
    df_expr.variant.operation.operands = &i32_arg1; // non-pointer -> lvalue expression fails
    assert(llvm_lower_expression(&df_expr, &val) != llvm_gen_be_error_t::ok);

    an_expr_node vac_e = {};
    vac_e.kind = enk_operation;
    vac_e.type = &void_ty;
    vac_e.variant.operation.kind = eok_dot_vacuous_destructor_call;
    vac_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&vac_e, &val) != llvm_gen_be_error_t::ok);

    real_e.is_lvalue = FALSE;
    real_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&real_e, &val) != llvm_gen_be_error_t::ok);

    // eok_va_copy resolve_va_list_ptr errors
    va_op_expr.variant.operation.kind = eok_va_copy;
    va_op_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&va_op_expr, &va_out) != llvm_gen_be_error_t::ok);
    va_var_node.next = &bad_op;
    va_op_expr.variant.operation.operands = &va_var_node;
    assert(llvm_lower_expression(&va_op_expr, &va_out) != llvm_gen_be_error_t::ok);
    va_var_node.next = nullptr;

    // eok_question errors
    bad_op.next = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    q_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    bool_cond.next = &bad_op;
    bad_op.next = &i32_arg2;
    q_e.variant.operation.operands = &bool_cond;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    bool_cond.next = &i32_arg1;
    i32_arg1.next = &bad_op;
    bad_op.next = nullptr;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &i32_arg2;
    q_e.type = &bad_ty;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // eok_assign errors
    bad_op.next = &i32_arg1;
    assign_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&assign_expr, &val) != llvm_gen_be_error_t::ok);
    vla_ptr_op.next = &i32_arg1;
    assign_expr.variant.operation.operands = &vla_ptr_op;
    assign_expr.type = &bad_ty;
    assert(llvm_lower_expression(&assign_expr, &val) != llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;

    // bok_builtin_bit_cast & enk_statement & enk_throw & enk_temp_init & enk_object_lifetime & enk_new_delete errors
    bok_op1.next = &bad_op;
    bok_e.variant.builtin_operation.kind = bok_builtin_bit_cast;
    bok_e.variant.builtin_operation.operands = &bok_op1;
    assert(llvm_lower_expression(&bok_e, &val) != llvm_gen_be_error_t::ok);
    bok_op1.next = &i32_arg1;
    bok_e.type = &bad_ty;
    assert(llvm_lower_expression(&bok_e, &val) != llvm_gen_be_error_t::ok);

    stmt_e.type = &bad_ty;
    assert(llvm_lower_expression(&stmt_e, &val) != llvm_gen_be_error_t::ok);

    t_supp.type = &bad_ty;
    t_supp.dynamic_init = &exc_dip;
    assert(llvm_lower_expression(&throw_e, &val) != llvm_gen_be_error_t::ok);
    t_supp.type = &int_type;
    t_supp.dynamic_init = &bad_dip;
    assert(llvm_lower_expression(&throw_e, &val) != llvm_gen_be_error_t::ok);
    a_type null_ti_ty = {};
    null_ti_ty.size = 4;
    t_supp.type = &null_ti_ty;
    t_supp.dynamic_init = nullptr;
    // get_typeinfo_global with nullptr type:
    t_supp.type = nullptr;

    empty_temp_e.type = &bad_ty;
    assert(llvm_lower_expression(&empty_temp_e, &val) != llvm_gen_be_error_t::ok);
    empty_temp_e.type = &int_type;
    empty_temp_e.variant.init.dynamic_init = &bad_dip;
    assert(llvm_lower_expression(&empty_temp_e, &val) != llvm_gen_be_error_t::ok);

    obj_lifetime_expr.variant.object_lifetime.expr = &bad_op;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) != llvm_gen_be_error_t::ok);

    custom_nd.type = &bad_ty;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.type = &int_type;
    custom_nd.placement_new = FALSE;
    custom_nd.number_of_elements = &bad_op;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.number_of_elements = nullptr;
    custom_nd.arg = &bad_op;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.arg = nullptr;
    bad_nd_e.type = &bad_ty;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    bad_nd_e.type = &ptr_type;
    custom_nd.dynamic_init = &bad_dip;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.freeing_of_storage_on_exception = &new_free_dip;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);

    custom_nd.is_new = FALSE;
    custom_nd.arg = &bad_op;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.next = &bad_op;
    bad_op.next = nullptr;
    custom_nd.arg = &ptr_arg1;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.next = nullptr;

    // 9. llvm_lower_compound_assignment error percolation
    ca_expr.variant.operation.kind = eok_pre_incr;
    ca_expr.variant.operation.operands = &bad_lval;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    local_bad_ty_var.is_lvalue = TRUE;
    ca_expr.variant.operation.operands = &local_bad_ty_var;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    ca_lhs = dummy_op1;
    ca_lhs.type = &int_type;
    ca_lhs.next = &bad_op;
    ca_expr.variant.operation.kind = eok_add_assign;
    ca_expr.variant.operation.operands = &ca_lhs;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);

    // 10. Phase 3 Expression Features:
    // - broadcast_scalar_to_vector (both t1=vec,t2=scalar and t1=scalar,t2=vec; fp->fp, int->fp, int->int)
    a_type vec_type = {}; vec_type.kind = tk_integer; vec_type.size = 16;
    llvm::Type* llvm_v4i32 = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
    state.type_cache[&vec_type] = llvm_v4i32;
    a_variable vec_var = {}; vec_var.type = &vec_type;
    state.local_vars[&vec_var] = state.builder->CreateAlloca(llvm_v4i32);

    a_type f32_vec_ty = {}; f32_vec_ty.kind = tk_float; f32_vec_ty.size = 16;
    llvm::Type* llvm_v4f32 = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4);
    state.type_cache[&f32_vec_ty] = llvm_v4f32;
    a_variable f32_vec_var = {}; f32_vec_var.type = &f32_vec_ty;
    state.local_vars[&f32_vec_var] = state.builder->CreateAlloca(llvm_v4f32);

    a_type i64_edg_ty = {}; i64_edg_ty.kind = tk_integer; i64_edg_ty.size = 8; i64_edg_ty.variant.integer.int_kind = ik_long_long;
    a_variable i64_var = {}; i64_var.type = &i64_edg_ty;
    state.local_vars[&i64_var] = state.builder->CreateAlloca(llvm::Type::getInt64Ty(*state.context));

    a_type f64_edg_ty = {}; f64_edg_ty.kind = tk_float; f64_edg_ty.size = 8; f64_edg_ty.variant.float_kind = fk_double;
    a_variable f64_var = {}; f64_var.type = &f64_edg_ty;
    state.local_vars[&f64_var] = state.builder->CreateAlloca(llvm::Type::getDoubleTy(*state.context));

    an_expr_node fv_op = {}, iv_op = {}, i64_op = {}, f64_op = {};
    fv_op.kind = enk_variable; fv_op.type = &f32_vec_ty; fv_op.variant.variable.ptr = &f32_vec_var;
    iv_op.kind = enk_variable; iv_op.type = &vec_type; iv_op.variant.variable.ptr = &vec_var;
    i64_op.kind = enk_variable; i64_op.type = &i64_edg_ty; i64_op.variant.variable.ptr = &i64_var;
    f64_op.kind = enk_variable; f64_op.type = &f64_edg_ty; f64_op.variant.variable.ptr = &f64_var;

    an_expr_node vec_bin_e = {};
    vec_bin_e.kind = enk_operation;
    vec_bin_e.variant.operation.kind = eok_add;
    // a) f32_vec + f64 (fp->fp cast)
    vec_bin_e.type = &f32_vec_ty;
    fv_op.next = &f64_op; f64_op.next = nullptr;
    vec_bin_e.variant.operation.operands = &fv_op;
    assert(llvm_lower_expression(&vec_bin_e, &val) == llvm_gen_be_error_t::ok);
    // b) i64 + f32_vec (int->fp cast, scalar on left)
    i64_op.next = &fv_op; fv_op.next = nullptr;
    vec_bin_e.variant.operation.operands = &i64_op;
    assert(llvm_lower_expression(&vec_bin_e, &val) == llvm_gen_be_error_t::ok);
    // c) i32_vec + i64 (int->int cast)
    vec_bin_e.type = &vec_type;
    iv_op.next = &i64_op; i64_op.next = nullptr;
    vec_bin_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_expression(&vec_bin_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_lambda & dip->static_temp == TRUE (and rvalue load error when expr->type is bad_ty after lvalue succeeds)
    a_dynamic_init static_tmp_dip = {};
    static_tmp_dip.kind = dik_zero;
    static_tmp_dip.static_temp = TRUE;
    an_expr_node lam_e = {};
    lam_e.kind = enk_lambda;
    lam_e.type = &int_type;
    lam_e.variant.init.dynamic_init = &static_tmp_dip;
    assert(llvm_lower_expression(&lam_e, &val) == llvm_gen_be_error_t::ok);

    // - eok_parens and eok_lvalue (both lvalue and rvalue)
    an_expr_node par_e = {};
    par_e.kind = enk_operation;
    par_e.type = &int_type;
    par_e.variant.operation.kind = eok_parens;
    dummy_op1.next = nullptr;
    par_e.variant.operation.operands = &dummy_op1;
    par_e.is_lvalue = TRUE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);
    par_e.is_lvalue = FALSE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);
    par_e.variant.operation.kind = eok_lvalue;
    par_e.is_lvalue = TRUE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);
    par_e.is_lvalue = FALSE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);

    // - eok_gnu_min and eok_gnu_max (fp, signed int, unsigned int, non-integer pointer)
    an_expr_node minmax_e = {};
    minmax_e.kind = enk_operation;
    minmax_e.type = &f64_edg_ty;
    an_expr_node f64_b = f64_op; f64_b.next = nullptr;
    f64_op.next = &f64_b;
    minmax_e.variant.operation.operands = &f64_op;
    minmax_e.variant.operation.kind = eok_gnu_min;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    minmax_e.variant.operation.kind = eok_gnu_max;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node i_op_a = i64_op, i_op_b = i64_op;
    i64_edg_ty.variant.integer.int_kind = ik_int;
    i_op_a.next = &i_op_b; i_op_b.next = nullptr;
    minmax_e.type = &i64_edg_ty;
    minmax_e.variant.operation.operands = &i_op_a;
    int_kind_is_signed[ik_int] = TRUE;
    minmax_e.variant.operation.kind = eok_gnu_min;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    minmax_e.variant.operation.kind = eok_gnu_max;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    int_kind_is_signed[ik_int] = FALSE;
    minmax_e.variant.operation.kind = eok_gnu_min;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    minmax_e.variant.operation.kind = eok_gnu_max;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    int_kind_is_signed[ik_int] = TRUE;
    i64_edg_ty.variant.integer.int_kind = ik_long_long;

    // - eok_dot_pm_func_ptr and eok_points_to_pm_func_ptr (struct pm_val, scalar pm_val, error propagation)
    a_type pmf_struct_edg_ty = {}; pmf_struct_edg_ty.kind = tk_ptr_to_member; pmf_struct_edg_ty.size = 16;
    llvm::Type* llvm_pmf_st = llvm::StructType::get(*state.context, {llvm::Type::getInt64Ty(*state.context), llvm::Type::getInt64Ty(*state.context)});
    state.type_cache[&pmf_struct_edg_ty] = llvm_pmf_st;
    a_variable pmf_var = {}; pmf_var.type = &pmf_struct_edg_ty;
    state.local_vars[&pmf_var] = state.builder->CreateAlloca(llvm_pmf_st);

    an_expr_node pmf_op = {}; pmf_op.kind = enk_variable; pmf_op.type = &pmf_struct_edg_ty; pmf_op.variant.variable.ptr = &pmf_var;
    an_expr_node pmfp_e = {}; pmfp_e.kind = enk_operation; pmfp_e.type = &ptr_type;
    pmfp_e.variant.operation.kind = eok_dot_pm_func_ptr;
    dummy_op1.next = &pmf_op; pmf_op.next = nullptr;
    pmfp_e.variant.operation.operands = &dummy_op1;
    assert(llvm_lower_expression(&pmfp_e, &val) == llvm_gen_be_error_t::ok);
    pmfp_e.variant.operation.kind = eok_points_to_pm_func_ptr;
    dummy_op1.next = &dummy_op2; dummy_op2.next = nullptr;
    assert(llvm_lower_expression(&pmfp_e, &val) == llvm_gen_be_error_t::ok);
    dummy_op1.next = &bad_op; bad_op.next = nullptr;
    assert(llvm_lower_expression(&pmfp_e, &val) != llvm_gen_be_error_t::ok);

    // - eok_vector_subscript (rvalue, eok_assign lvalue, and compound assignment lvalue + error paths)
    an_expr_node vsub_e = {}; vsub_e.kind = enk_operation; vsub_e.type = &int_type;
    vsub_e.variant.operation.kind = eok_vector_subscript;
    iv_op.next = &dummy_op2; dummy_op2.next = nullptr;
    vsub_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_expression(&vsub_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node vass_e = {}; vass_e.kind = enk_operation; vass_e.type = &int_type;
    vass_e.variant.operation.kind = eok_assign;
    vsub_e.next = &dummy_op1; dummy_op1.next = nullptr;
    vass_e.variant.operation.operands = &vsub_e;
    assert(llvm_lower_expression(&vass_e, &val) == llvm_gen_be_error_t::ok);
    // Error paths in eok_assign vector_subscript:
    vsub_e.next = &bad_op;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);
    iv_op.next = &bad_op;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);
    vsub_e.variant.operation.operands = &bad_lval;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);
    an_expr_node bad_ty_vbase = iv_op; bad_ty_vbase.type = &bad_ty; bad_ty_vbase.next = &dummy_op2;
    vsub_e.variant.operation.operands = &bad_ty_vbase;
    vsub_e.next = &dummy_op1;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);

    // Compound assignment on eok_vector_subscript:
    iv_op.next = &dummy_op2; dummy_op2.next = nullptr;
    vsub_e.variant.operation.operands = &iv_op;
    vsub_e.next = &dummy_op1; dummy_op1.next = nullptr;
    ca_expr.variant.operation.kind = eok_add_assign;
    ca_expr.variant.operation.operands = &vsub_e;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    // Error paths on compound assignment eok_vector_subscript:
    vsub_e.variant.operation.operands = &bad_ty_vbase;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    iv_op.next = &bad_op;
    vsub_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    vsub_e.variant.operation.operands = &bad_lval;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);

    // - eok_vector_question (success and error in true_expr/false_expr)
    an_expr_node vq_e = {}; vq_e.kind = enk_operation; vq_e.type = &vec_type;
    vq_e.variant.operation.kind = eok_vector_question;
    an_expr_node iv_op2 = iv_op, iv_op3 = iv_op;
    iv_op.next = &iv_op2; iv_op2.next = &iv_op3; iv_op3.next = nullptr;
    vq_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_expression(&vq_e, &val) == llvm_gen_be_error_t::ok);
    iv_op2.next = &bad_op; bad_op.next = nullptr;
    assert(llvm_lower_expression(&vq_e, &val) != llvm_gen_be_error_t::ok);

    // - enk_builtin_choose_expr (invalid args, choose_first TRUE and FALSE)
    an_expr_node bce_e = {}; bce_e.kind = enk_builtin_choose_expr; bce_e.type = &int_type;
    bce_e.variant.builtin_choose_expr.operands = nullptr;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::invalid_argument);
    dummy_op1.next = nullptr;
    bce_e.variant.builtin_choose_expr.operands = &dummy_op1;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::invalid_argument);
    dummy_op1.next = &dummy_op2; dummy_op2.next = nullptr;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node dummy_op3 = dummy_op1; dummy_op3.next = nullptr;
    dummy_op2.next = &dummy_op3;
    bce_e.variant.builtin_choose_expr.choose_first = TRUE;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::ok);
    bce_e.variant.builtin_choose_expr.choose_first = FALSE;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_condition (null supp, initialization ok/fail, dynamic_init new/cached var, bad type, bad dip)
    an_expr_node cond_e = {}; cond_e.kind = enk_condition; cond_e.type = &int_type;
    cond_e.variant.condition = nullptr;
    assert(llvm_lower_expression(&cond_e, &val) == llvm_gen_be_error_t::invalid_argument);

    a_statement cond_ok_stmt = {}; cond_ok_stmt.kind = stmk_empty;
    a_statement cond_bad_stmt = {}; cond_bad_stmt.kind = stmk_return;
    a_variable cond_new_var = {}; cond_new_var.type = &int_type;
    a_dynamic_init cond_dip = {}; cond_dip.kind = dik_zero; cond_dip.variable = nullptr;
    a_condition_supplement cond_supp = {};
    cond_supp.expr = &dummy_op1; dummy_op1.next = nullptr;
    cond_e.variant.condition = &cond_supp;
    assert(llvm_lower_expression(&cond_e, &val) == llvm_gen_be_error_t::ok);

    cond_supp.initialization = &cond_bad_stmt;
    assert(llvm_lower_expression(&cond_e, &val) != llvm_gen_be_error_t::ok);
    cond_supp.initialization = &cond_ok_stmt;
    // Wait: in test_llvm_expressions.cpp stub, llvm_emit_dynamic_init fails when dip->variable != nullptr!
    // Let's temporarily use a variable with bad_ty first to test get_llvm_type failure:
    a_variable cond_bad_ty_var = {}; cond_bad_ty_var.type = &bad_ty;
    cond_dip.variable = &cond_bad_ty_var;
    cond_supp.dynamic_init = &cond_dip;
    assert(llvm_lower_expression(&cond_e, &val) != llvm_gen_be_error_t::ok);
    // Now cond_new_var (not in local_vars -> allocates and fails in stub llvm_emit_dynamic_init because dip->variable != nullptr)
    cond_dip.variable = &cond_new_var;
    assert(llvm_lower_expression(&cond_e, &val) != llvm_gen_be_error_t::ok);
    // Second call finds cond_new_var in local_vars!
    cond_dip.kind = dik_constant; // we'll allow dik_constant with variable in stub below
    assert(llvm_lower_expression(&cond_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_field (null ptr and valid ptr)
    an_expr_node fld_e = {}; fld_e.kind = enk_field; fld_e.type = &int_type;
    fld_e.variant.field.ptr = nullptr;
    assert(llvm_lower_expression(&fld_e, &val) == llvm_gen_be_error_t::invalid_argument);
    a_field test_fld = {}; test_fld.offset = 12;
    fld_e.variant.field.ptr = &test_fld;
    assert(llvm_lower_expression(&fld_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_sizeof, enk_alignof, enk_datasizeof, enk_sizeof_pack, enk_initializer
    an_expr_node sz_e = {}; sz_e.kind = enk_sizeof; sz_e.type = &int_type;
    sz_e.variant.sizeof_info.is_type = TRUE;
    sz_e.variant.sizeof_info.variant.type = &int_type;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    sz_e.variant.sizeof_info.is_type = FALSE;
    sz_e.variant.sizeof_info.variant.expr = &dummy_op1;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    sz_e.variant.sizeof_info.variant.expr = nullptr;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);

    sz_e.kind = enk_alignof;
    sz_e.variant.sizeof_info.is_type = TRUE;
    sz_e.variant.sizeof_info.variant.type = &int_type;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    sz_e.variant.sizeof_info.variant.type = nullptr;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);

    // VLA sizeof: 3D array (mapped VLA dim, unmapped VLA dim, fixed dim > 0, fixed dim == 0) with element size > 1 and == 1
    a_vla_dimension vdim_mapped = {}, vdim_unmapped = {};
    state.vla_dimension_values[&vdim_mapped] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 5);
    a_type vla_el_ty = int_type; vla_el_ty.size = 4;
    a_type vla_el1_ty = int_type; vla_el1_ty.size = 1;
    a_type vla_a4 = {}; vla_a4.kind = tk_array; vla_a4.variant.array.element_type = &vla_el_ty; vla_a4.variant.array.variant.number_of_elements = 0;
    a_type vla_a3 = {}; vla_a3.kind = tk_array; vla_a3.variant.array.element_type = &vla_a4; vla_a3.variant.array.variant.number_of_elements = 3;
    a_type vla_a2 = {}; vla_a2.kind = tk_array; vla_a2.variant.array.element_type = &vla_a3;
    a_type vla_a1 = {}; vla_a1.kind = tk_array; vla_a1.variant.array.element_type = &vla_a2;
    state.array_to_vla_dim[&vla_a1] = &vdim_mapped;
    state.array_to_vla_dim[&vla_a2] = &vdim_unmapped;

    sz_e.kind = enk_datasizeof;
    sz_e.variant.sizeof_info.is_type = TRUE;
    sz_e.variant.sizeof_info.variant.type = &vla_a1;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    vla_a4.variant.array.element_type = &vla_el1_ty;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node sz_pack_e = {}; sz_pack_e.kind = enk_sizeof_pack; sz_pack_e.type = &int_type;
    assert(llvm_lower_expression(&sz_pack_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node init_e = {}; init_e.kind = enk_initializer; init_e.type = &int_type;
    init_e.variant.initializer.dyn_init = nullptr;
    assert(llvm_lower_expression(&init_e, &val) == llvm_gen_be_error_t::invalid_argument);
    a_dynamic_init ok_init_dip = {}; ok_init_dip.kind = dik_zero;
    init_e.variant.initializer.dyn_init = &ok_init_dip;
    assert(llvm_lower_expression(&init_e, &val) == llvm_gen_be_error_t::ok);
    init_e.type = &bad_ty;
    assert(llvm_lower_expression(&init_e, &val) != llvm_gen_be_error_t::ok);
    init_e.type = &int_type;
    init_e.variant.initializer.dyn_init = &bad_dip;
    assert(llvm_lower_expression(&init_e, &val) != llvm_gen_be_error_t::ok);

    // - Complex compound assignment (+=, -=, *=, /=) with complex RHS, float RHS, and int RHS
    a_type cplx_edg_ty = {}; cplx_edg_ty.kind = tk_complex; cplx_edg_ty.size = 8; cplx_edg_ty.variant.float_kind = fk_float;
    llvm::Type* llvm_cplx_f32 = llvm::StructType::get(*state.context, {llvm::Type::getFloatTy(*state.context), llvm::Type::getFloatTy(*state.context)});
    state.type_cache[&cplx_edg_ty] = llvm_cplx_f32;
    a_variable cplx_var = {}; cplx_var.type = &cplx_edg_ty;
    state.local_vars[&cplx_var] = state.builder->CreateAlloca(llvm_cplx_f32);

    an_expr_node cplx_lhs = {}, cplx_rhs = {};
    cplx_lhs.kind = enk_variable; cplx_lhs.type = &cplx_edg_ty; cplx_lhs.variant.variable.ptr = &cplx_var;
    cplx_rhs.kind = enk_variable; cplx_rhs.type = &cplx_edg_ty; cplx_rhs.variant.variable.ptr = &cplx_var;

    ca_expr.type = &cplx_edg_ty;
    ca_expr.variant.operation.operands = &cplx_lhs;
    cplx_lhs.next = &cplx_rhs; cplx_rhs.next = nullptr;
    for (an_expr_operator_kind ck : {eok_add_assign, eok_subtract_assign, eok_multiply_assign, eok_divide_assign}) {
      ca_expr.variant.operation.kind = ck;
      assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    }
    // Complex += float and Complex += int
    cplx_lhs.next = &f64_op; f64_op.next = nullptr;
    ca_expr.variant.operation.kind = eok_add_assign;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    cplx_lhs.next = &dummy_op1; dummy_op1.next = nullptr;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_new_delete with active landing pad (invoke) and nounwind operator new
    llvm::Function* cur_f = state.builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* new_lpad = llvm::BasicBlock::Create(*state.context, "new.lpad", cur_f);
    state.current_landing_pads.push_back(new_lpad);
    custom_nd.is_new = TRUE;
    custom_nd.type = &int_type;
    custom_nd.dynamic_init = nullptr;
    custom_nd.freeing_of_storage_on_exception = nullptr;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::ok);
    llvm::Function* op_new_fn = state.module->getFunction(custom_nd.routine->source_corresp.name);
    if (op_new_fn) op_new_fn->setDoesNotThrow();
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.pop_back();

    // Also test llvm_lower_call_expression when callee is a nounwind Function
    an_expr_node nw_callee = {};
    nw_callee.kind = enk_routine;
    nw_callee.variant.routine.ptr = custom_nd.routine;
    an_expr_node nw_call = {};
    nw_call.kind = enk_operation;
    nw_call.type = &int_type;
    nw_call.variant.operation.kind = eok_call;
    nw_call.variant.operation.operands = &nw_callee;
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    // Test multi-register (2-eightbyte struct + trailing int) aggregate argument packing in llvm_lower_call_expression
    llvm::Type* i64_llvm_ty = llvm::Type::getInt64Ty(*state.context);
    llvm::StructType* pair64_llvm_ty = llvm::StructType::get(*state.context, {i64_llvm_ty, i64_llvm_ty});
    a_type pair64_edg_ty = {};
    pair64_edg_ty.kind = tk_struct;
    pair64_edg_ty.size = 16;
    pair64_edg_ty.alignment = 8;
    a_field pf1 = {}, pf2 = {};
    pf1.type = &i64_ty; pf1.offset = 0; pf1.next = &pf2;
    pf2.type = &i64_ty; pf2.offset = 8; pf2.next = nullptr;
    pair64_edg_ty.variant.class_struct_union.field_list = &pf1;
    state.type_cache[&pair64_edg_ty] = pair64_llvm_ty;
    a_variable pair64_var = {};
    pair64_var.type = &pair64_edg_ty;
    state.local_vars[&pair64_var] = state.builder->CreateAlloca(pair64_llvm_ty);

    a_param_type hfa_p1 = {}, hfa_p2 = {};
    hfa_p1.type = &pair64_edg_ty;
    hfa_p1.next = &hfa_p2;
    hfa_p2.type = &int_type;
    a_routine_type_supplement hfa_rsupp = {};
    hfa_rsupp.param_type_list = &hfa_p1;
    a_type hfa_rout_ty = {};
    hfa_rout_ty.kind = tk_routine;
    hfa_rout_ty.variant.routine.return_type = &int_type;
    hfa_rout_ty.variant.routine.extra_info = &hfa_rsupp;

    llvm::FunctionType* hfa4_callee_fty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*state.context), {i64_llvm_ty, i64_llvm_ty, llvm::Type::getInt32Ty(*state.context)}, false);
    llvm::Function::Create(hfa4_callee_fty, llvm::GlobalValue::ExternalLinkage, "hfa4_callee", state.module.get());
    a_routine hfa4_rout = {};
    hfa4_rout.source_corresp.name = "hfa4_callee";
    hfa4_rout.type = &hfa_rout_ty;

    a_constant hfa4_c_int = {};
    hfa4_c_int.kind = ck_integer;
    hfa4_c_int.type = &int_type;
    an_expr_node hfa4_callee_node = {}, hfa4_arg1 = {}, hfa4_arg2 = {};
    hfa4_callee_node.kind = enk_routine;
    hfa4_callee_node.type = &hfa_rout_ty;
    hfa4_callee_node.variant.routine.ptr = &hfa4_rout;
    hfa4_callee_node.next = &hfa4_arg1;
    hfa4_arg1.kind = enk_variable;
    hfa4_arg1.type = &pair64_edg_ty;
    hfa4_arg1.variant.variable.ptr = &pair64_var;
    hfa4_arg1.next = &hfa4_arg2;
    hfa4_arg2.kind = enk_constant;
    hfa4_arg2.type = &int_type;
    hfa4_arg2.variant.constant.ptr = &hfa4_c_int;
    nw_call.variant.operation.operands = &hfa4_callee_node;
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    // Also test when hfa4_callee has only 2 params (so 16-byte struct coerced to 1st i64 stops before consuming 2nd param), and when op1 is non-enk_routine
    llvm::Function* hfa4_fn = state.module->getFunction("hfa4_callee");
    if (hfa4_fn) hfa4_fn->eraseFromParent();
    llvm::FunctionType* hfa2_callee_fty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*state.context), {i64_llvm_ty, llvm::Type::getInt32Ty(*state.context)}, false);
    llvm::Function::Create(hfa2_callee_fty, llvm::GlobalValue::ExternalLinkage, "hfa4_callee", state.module.get());
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    a_type ptr_hfa_rout_ty = {};
    ptr_hfa_rout_ty.kind = tk_pointer;
    ptr_hfa_rout_ty.size = 8;
    ptr_hfa_rout_ty.variant.pointer.type = &hfa_rout_ty;
    state.type_cache[&ptr_hfa_rout_ty] = llvm::PointerType::getUnqual(*state.context);
    a_variable fn_ptr_var = {};
    fn_ptr_var.type = &ptr_hfa_rout_ty;
    state.local_vars[&fn_ptr_var] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    hfa4_callee_node.kind = enk_variable;
    hfa4_callee_node.type = &ptr_hfa_rout_ty;
    hfa4_callee_node.variant.variable.ptr = &fn_ptr_var;
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    // Phase 17:
    // 1. x86_32, arm32, thumb, and wasm eok_va_{start,arg,end,copy} dispatch
    a_variable va_v17 = {};
    va_v17.source_corresp.name = "va_v17";
    va_v17.type = &ptr_type;
    state.local_vars[&va_v17] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    an_expr_node va_op1_17 = {}, va_op2_17 = {}, va_expr_17 = {};
    va_op1_17.kind = enk_variable;
    va_op1_17.type = &ptr_type;
    va_op1_17.variant.variable.ptr = &va_v17;
    va_op2_17 = va_op1_17;
    va_op1_17.next = &va_op2_17;
    va_expr_17.kind = enk_operation;
    va_expr_17.type = &int_type;
    va_expr_17.variant.operation.operands = &va_op1_17;

    const char* va_triples_17[] = {
        "i686-unknown-linux-gnu",
        "armv7-unknown-linux-gnueabihf",
        "thumbv7-unknown-linux-gnueabihf",
        "wasm32-unknown-unknown"
    };
    const an_expr_operator_kind va_ops_17[] = {eok_va_start, eok_va_arg, eok_va_end, eok_va_copy};
    for (const char* t_str : va_triples_17) {
      state.module->setTargetTriple(llvm::Triple(t_str));
      for (an_expr_operator_kind vk : va_ops_17) {
        va_expr_17.variant.operation.kind = vk;
        assert(llvm_lower_expression(&va_expr_17, &val) == llvm_gen_be_error_t::ok);
      }
    }
    state.module->setTargetTriple(llvm::Triple("x86_64-unknown-linux-gnu"));

    // 2. C11 _Atomic (TQ_C11_ATOMIC) loads and stores (scalar sizes 1, 2, 4, 8, 16, pointer, volatile, and non-power-of-two/aggregate)
    unsigned atomic_sizes[] = {1, 2, 4, 8, 16};
    for (unsigned asz : atomic_sizes) {
      a_type base_int_at = {};
      base_int_at.kind = tk_integer;
      base_int_at.size = asz;
      state.type_cache[&base_int_at] = llvm::Type::getIntNTy(*state.context, asz * 8);
      a_type atomic_int_ty = {};
      atomic_int_ty.kind = tk_typeref;
      atomic_int_ty.size = asz;
      atomic_int_ty.variant.typeref.type = &base_int_at;
      atomic_int_ty.variant.typeref.qualifiers = (asz == 4) ? (TQ_C11_ATOMIC | TQ_VOLATILE) : TQ_C11_ATOMIC;
      state.type_cache[&atomic_int_ty] = llvm::Type::getIntNTy(*state.context, asz * 8);

      a_variable at_var = {};
      at_var.source_corresp.name = "at_var";
      at_var.type = &atomic_int_ty;
      state.local_vars[&at_var] = state.builder->CreateAlloca(llvm::Type::getIntNTy(*state.context, asz * 8));

      an_expr_node at_var_node = {};
      at_var_node.kind = enk_variable;
      at_var_node.type = &atomic_int_ty;
      at_var_node.variant.variable.ptr = &at_var;
      assert(llvm_lower_expression(&at_var_node, &val) == llvm_gen_be_error_t::ok);

      an_expr_node at_assign = {};
      at_assign.kind = enk_operation;
      at_assign.type = &atomic_int_ty;
      at_assign.variant.operation.kind = eok_assign;
      an_expr_node at_rhs = at_var_node;
      at_var_node.next = &at_rhs;
      at_assign.variant.operation.operands = &at_var_node;
      assert(llvm_lower_expression(&at_assign, &val) == llvm_gen_be_error_t::ok);
    }

    // _Atomic pointer load/store
    a_type atomic_ptr_ty = {};
    atomic_ptr_ty.kind = tk_typeref;
    atomic_ptr_ty.size = 8;
    atomic_ptr_ty.variant.typeref.type = &ptr_type;
    atomic_ptr_ty.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    state.type_cache[&atomic_ptr_ty] = llvm::PointerType::getUnqual(*state.context);
    a_variable at_ptr_var = {};
    at_ptr_var.type = &atomic_ptr_ty;
    state.local_vars[&at_ptr_var] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    an_expr_node at_ptr_lhs = {}, at_ptr_rhs = {}, at_ptr_assign = {};
    at_ptr_lhs.kind = enk_variable;
    at_ptr_lhs.type = &atomic_ptr_ty;
    at_ptr_lhs.variant.variable.ptr = &at_ptr_var;
    at_ptr_rhs = at_ptr_lhs;
    at_ptr_lhs.next = &at_ptr_rhs;
    at_ptr_assign.kind = enk_operation;
    at_ptr_assign.type = &atomic_ptr_ty;
    at_ptr_assign.variant.operation.kind = eok_assign;
    at_ptr_assign.variant.operation.operands = &at_ptr_lhs;
    assert(llvm_lower_expression(&at_ptr_assign, &val) == llvm_gen_be_error_t::ok);

    // _Atomic aggregate (non-power-of-two size 24 struct -> __atomic_load / __atomic_store) and _Atomic 3-byte integer
    llvm::StructType* aggr24_llvm_ty = llvm::StructType::get(
        *state.context, {i64_llvm_ty, i64_llvm_ty, i64_llvm_ty});
    a_type aggr24_edg = {};
    aggr24_edg.kind = tk_struct;
    aggr24_edg.size = 24;
    a_type atomic_aggr24_ty = {};
    atomic_aggr24_ty.kind = tk_typeref;
    atomic_aggr24_ty.size = 24;
    atomic_aggr24_ty.variant.typeref.type = &aggr24_edg;
    atomic_aggr24_ty.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    state.type_cache[&atomic_aggr24_ty] = aggr24_llvm_ty;
    a_variable at_aggr_var = {};
    at_aggr_var.type = &atomic_aggr24_ty;
    state.local_vars[&at_aggr_var] = state.builder->CreateAlloca(aggr24_llvm_ty);
    an_expr_node at_aggr_lhs = {}, at_aggr_rhs = {}, at_aggr_assign = {};
    at_aggr_lhs.kind = enk_variable;
    at_aggr_lhs.type = &atomic_aggr24_ty;
    at_aggr_lhs.variant.variable.ptr = &at_aggr_var;
    at_aggr_rhs = at_aggr_lhs;
    at_aggr_lhs.next = &at_aggr_rhs;
    at_aggr_assign.kind = enk_operation;
    at_aggr_assign.type = &atomic_aggr24_ty;
    at_aggr_assign.variant.operation.kind = eok_assign;
    at_aggr_assign.variant.operation.operands = &at_aggr_lhs;
    assert(llvm_lower_expression(&at_aggr_assign, &val) == llvm_gen_be_error_t::ok);

    a_type atomic_i24_ty = {};
    atomic_i24_ty.kind = tk_typeref;
    atomic_i24_ty.size = 3;
    atomic_i24_ty.variant.typeref.type = &int_type;
    atomic_i24_ty.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    state.type_cache[&atomic_i24_ty] = llvm::Type::getIntNTy(*state.context, 24);
    a_variable at_i24_var = {};
    at_i24_var.type = &atomic_i24_ty;
    state.local_vars[&at_i24_var] = state.builder->CreateAlloca(llvm::Type::getIntNTy(*state.context, 24));
    at_aggr_lhs.type = &atomic_i24_ty;
    at_aggr_lhs.variant.variable.ptr = &at_i24_var;
    at_aggr_rhs.type = &atomic_i24_ty;
    at_aggr_rhs.variant.variable.ptr = &at_i24_var;
    at_aggr_assign.type = &atomic_i24_ty;
    assert(llvm_lower_expression(&at_aggr_assign, &val) == llvm_gen_be_error_t::ok);

    // 3. Fixed-point binary arithmetic (llvm_lower_fixed_point_binop)
    llvm::Value* fx_lhs = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 100);
    llvm::Value* fx_rhs = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 25);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, nullptr, fx_rhs, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, fx_lhs, nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, fx_lhs, fx_rhs, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_add, true, true, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_add, false, true, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_subtract, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_subtract, true, true, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_subtract, false, true, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, false, false, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, true, true, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, false, true, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_divide, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_divide, false, false, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_remainder, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::unsupported_expr);

    // Phase 18:
    // 1. Front-end UBSan checks (add/sub/mul overflow, div/rem zero, shiftl/shiftr bounds, null load)
    state.enable_ubsan_checks = true;
    int_kind_is_signed[ik_int] = TRUE;
    int_type.variant.integer.int_kind = ik_int;
    an_expr_node ubsan_op1 = {}, ubsan_op2 = {}, ubsan_bin = {};
    a_variable ubsan_var = {};
    ubsan_var.source_corresp.name = "ubsan_var";
    ubsan_var.type = &int_type;
    state.local_vars[&ubsan_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    ubsan_op1.kind = enk_variable;
    ubsan_op1.type = &int_type;
    ubsan_op1.variant.variable.ptr = &ubsan_var;
    ubsan_op2 = ubsan_op1;
    ubsan_op1.next = &ubsan_op2;
    ubsan_bin.kind = enk_operation;
    ubsan_bin.type = &int_type;
    ubsan_bin.variant.operation.operands = &ubsan_op1;

    const an_expr_operator_kind ubsan_arith_ops[] = {
        eok_negate, eok_add, eok_subtract, eok_multiply, eok_divide, eok_remainder, eok_shiftl, eok_shiftr
    };
    assert(llvm_emit_ubsan_check(nullptr, val, {}) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_ubsan_check("__ubsan_handle_out_of_bounds", nullptr, {}) == llvm_gen_be_error_t::invalid_argument);
    for (an_expr_operator_kind aop : ubsan_arith_ops) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    // Also test unsigned negate/add/sub/mul and non-tk_integer with enable_ubsan_checks == true
    int_kind_is_signed[ik_int] = FALSE;
    for (an_expr_operator_kind aop : {eok_negate, eok_add, eok_subtract, eok_multiply}) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    int_kind_is_signed[ik_int] = TRUE;
    a_type enum_int_ty = int_type;
    enum_int_ty.kind = tk_enum;
    state.type_cache[&enum_int_ty] = llvm::Type::getInt32Ty(*state.context);
    ubsan_op1.type = &enum_int_ty;
    for (an_expr_operator_kind aop : {eok_add, eok_subtract, eok_multiply}) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    ubsan_op1.type = &int_type;
    // Also test vector add/sub/mul/div/rem/shiftl/shiftr with enable_ubsan_checks == true
    llvm::Type* vec2i32_ty = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 2);
    a_type edg_vec2i32_ty = {};
    edg_vec2i32_ty.kind = tk_integer;
    edg_vec2i32_ty.size = 8;
    state.type_cache[&edg_vec2i32_ty] = vec2i32_ty;
    a_variable ubsan_vec_var = {};
    ubsan_vec_var.type = &edg_vec2i32_ty;
    state.local_vars[&ubsan_vec_var] = state.builder->CreateAlloca(vec2i32_ty);
    ubsan_op1.type = &edg_vec2i32_ty;
    ubsan_op1.variant.variable.ptr = &ubsan_vec_var;
    ubsan_op2.type = &edg_vec2i32_ty;
    ubsan_op2.variant.variable.ptr = &ubsan_vec_var;
    ubsan_bin.type = &edg_vec2i32_ty;
    for (an_expr_operator_kind aop : ubsan_arith_ops) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    state.enable_ubsan_checks = false;

    // 2. TBAA metadata (!tbaa and !tbaa.struct) when opt_level != O0
    state.opt_level = llvm_opt_level_t::O2;
    ubsan_op1.type = &int_type;
    ubsan_op1.variant.variable.ptr = &ubsan_var;
    ubsan_op2.type = &int_type;
    ubsan_op2.variant.variable.ptr = &ubsan_var;
    ubsan_bin.type = &int_type;
    ubsan_bin.variant.operation.kind = eok_assign;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    // Float and pointer load/store with TBAA
    a_variable flt_tbaa_var = {};
    flt_tbaa_var.type = &float_type;
    state.local_vars[&flt_tbaa_var] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));
    ubsan_op1.type = &float_type;
    ubsan_op1.variant.variable.ptr = &flt_tbaa_var;
    ubsan_op2.type = &float_type;
    ubsan_op2.variant.variable.ptr = &flt_tbaa_var;
    ubsan_bin.type = &float_type;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    ubsan_op1.type = &ptr_type;
    ubsan_op1.variant.variable.ptr = &va_v17;
    ubsan_op2.type = &ptr_type;
    ubsan_op2.variant.variable.ptr = &va_v17;
    ubsan_bin.type = &ptr_type;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    // Vector (non-scalar, size <= 16) load/store with opt_level == O2
    ubsan_op1.type = &edg_vec2i32_ty;
    ubsan_op1.variant.variable.ptr = &ubsan_vec_var;
    ubsan_op2.type = &edg_vec2i32_ty;
    ubsan_op2.variant.variable.ptr = &ubsan_vec_var;
    ubsan_bin.type = &edg_vec2i32_ty;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    // Aggregate (> 16 bytes) assignment with opt_level == O2 -> !tbaa.struct on memcpy
    state.type_cache[&aggr24_edg] = aggr24_llvm_ty;
    a_variable aggr24_plain_var = {};
    aggr24_plain_var.type = &aggr24_edg;
    state.local_vars[&aggr24_plain_var] = state.builder->CreateAlloca(aggr24_llvm_ty);
    ubsan_op1.type = &aggr24_edg;
    ubsan_op1.variant.variable.ptr = &aggr24_plain_var;
    ubsan_op2.type = &aggr24_edg;
    ubsan_op2.variant.variable.ptr = &aggr24_plain_var;
    ubsan_bin.type = &aggr24_edg;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    state.opt_level = llvm_opt_level_t::O0;

    // 3. Cast to void, lvalue eok_lvalue_adjust/eok_lvalue_cast/eok_question, eok_address_of with is_lvalue, and variadic call
    a_type void_edg_ty = {};
    void_edg_ty.kind = tk_void;
    state.type_cache[&void_edg_ty] = llvm::Type::getVoidTy(*state.context);
    an_expr_node void_cast_op = {};
    void_cast_op.kind = enk_operation;
    void_cast_op.type = &void_edg_ty;
    void_cast_op.variant.operation.kind = eok_cast;
    ubsan_op1.type = &int_type;
    ubsan_op1.variant.variable.ptr = &ubsan_var;
    ubsan_op1.next = nullptr;
    void_cast_op.variant.operation.operands = &ubsan_op1;
    assert(llvm_lower_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);
    assert(val == nullptr);

    void_cast_op.type = &int_type;
    void_cast_op.variant.operation.kind = eok_lvalue_adjust;
    assert(llvm_lower_lvalue_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);
    void_cast_op.variant.operation.kind = eok_lvalue_cast;
    assert(llvm_lower_lvalue_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);

    // Lvalue eok_question (i32 cond, i1 cond, and error propagation on cond/true/false)
    an_expr_node lq_cond = {}, lq_true = {}, lq_false = {}, lq_expr = {};
    lq_cond.kind = enk_variable;
    lq_cond.type = &int_type;
    lq_cond.variant.variable.ptr = &ubsan_var;
    lq_true = lq_cond;
    lq_false = lq_cond;
    lq_cond.next = &lq_true;
    lq_true.next = &lq_false;
    lq_expr.kind = enk_operation;
    lq_expr.type = &int_type;
    lq_expr.is_lvalue = TRUE;
    lq_expr.variant.operation.kind = eok_question;
    lq_expr.variant.operation.operands = &lq_cond;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::ok);

    a_type i1_edg_ty = {};
    i1_edg_ty.kind = tk_integer;
    i1_edg_ty.size = 1;
    state.type_cache[&i1_edg_ty] = llvm::Type::getInt1Ty(*state.context);
    a_variable i1_var = {};
    i1_var.type = &i1_edg_ty;
    state.local_vars[&i1_var] = state.builder->CreateAlloca(llvm::Type::getInt1Ty(*state.context));
    lq_cond.type = &i1_edg_ty;
    lq_cond.variant.variable.ptr = &i1_var;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::ok);

    lq_false.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    lq_false.variant.variable.ptr = &ubsan_var;
    lq_true.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    lq_true.variant.variable.ptr = &ubsan_var;
    lq_cond.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    lq_cond.variant.variable.ptr = &i1_var;

    // eok_address_of with op1->is_lvalue = TRUE and op1->kind == enk_operation (eok_lvalue_adjust, eok_question, and other)
    an_expr_node addr_lv_op = {};
    addr_lv_op.kind = enk_operation;
    addr_lv_op.type = &ptr_type;
    addr_lv_op.variant.operation.kind = eok_address_of;
    lq_expr.next = nullptr;
    addr_lv_op.variant.operation.operands = &lq_expr;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);

    lq_expr.is_lvalue = FALSE;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);
    lq_expr.is_lvalue = TRUE;

    void_cast_op.variant.operation.kind = eok_lvalue_adjust;
    void_cast_op.is_lvalue = FALSE;
    addr_lv_op.variant.operation.operands = &void_cast_op;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);

    void_cast_op.variant.operation.kind = eok_parens;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);

    // Variadic call where callee_arg_idx >= callee_ty->getNumParams()
    llvm::FunctionType* vararg_fty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*state.context), {llvm::Type::getInt32Ty(*state.context)}, true);
    llvm::Function::Create(vararg_fty, llvm::GlobalValue::ExternalLinkage, "vararg_test_fn", state.module.get());
    a_routine vararg_rout = {};
    vararg_rout.source_corresp.name = "vararg_test_fn";
    vararg_rout.type = &rout_ty;
    state.type_cache[&rout_ty] = vararg_fty;
    an_expr_node va_callee_node = {}, va_arg1 = {}, va_arg2 = {}, va_call = {};
    va_callee_node.kind = enk_routine;
    va_callee_node.type = &rout_ty;
    va_callee_node.variant.routine.ptr = &vararg_rout;
    va_arg1.kind = enk_variable;
    va_arg1.type = &int_type;
    va_arg1.variant.variable.ptr = &ubsan_var;
    va_arg2 = va_arg1;
    va_callee_node.next = &va_arg1;
    va_arg1.next = &va_arg2;
    va_call.kind = enk_operation;
    va_call.type = &int_type;
    va_call.variant.operation.kind = eok_call;
    va_call.variant.operation.operands = &va_callee_node;
    assert(llvm_lower_expression(&va_call, &val) == llvm_gen_be_error_t::ok);

    // 4. Phase 2 tests: llvm_compute_type_padding_mask, __builtin_clear_padding, llvm_lower_sizeof_pack,
    //    llvm_lower_fold_expression (all 32 operators, short-circuit &&/||, signed/unsigned/float),
    //    llvm_lower_new_expression (eok_new), and llvm_lower_delete_expression (eok_delete)
    assert(llvm_compute_type_padding_mask(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    std::vector<uint8_t> pad_mask;
    assert(llvm_compute_type_padding_mask(nullptr, &pad_mask) == llvm_gen_be_error_t::ok);
    a_type zero_sz_struct = {};
    zero_sz_struct.kind = tk_struct;
    zero_sz_struct.size = 0;
    assert(llvm_compute_type_padding_mask(&zero_sz_struct, &pad_mask) == llvm_gen_be_error_t::ok);

    a_type base_pad_ty = {};
    base_pad_ty.kind = tk_integer;
    base_pad_ty.size = 2;
    a_base_class bc_pad1 = {}, bc_pad_null = {}, bc_pad_oob = {};
    bc_pad1.type = &base_pad_ty; bc_pad1.offset = 0; bc_pad1.next = &bc_pad_null;
    bc_pad_null.type = nullptr; bc_pad_null.next = &bc_pad_oob;
    bc_pad_oob.type = &base_pad_ty; bc_pad_oob.offset = 10; bc_pad_oob.next = nullptr;
    a_class_type_supplement pad_supp = {};
    pad_supp.base_classes = &bc_pad1;

    a_field pf_bf1 = {}, pf_bf_oob = {}, pf_norm = {}, pf_null = {}, pf_oob = {};
    pf_bf1.is_bit_field = TRUE; pf_bf1.offset = 2; pf_bf1.offset_bit_remainder = 0; pf_bf1.bit_size = 3; pf_bf1.next = &pf_bf_oob;
    pf_bf_oob.is_bit_field = TRUE; pf_bf_oob.offset = 8; pf_bf_oob.offset_bit_remainder = 0; pf_bf_oob.bit_size = 4; pf_bf_oob.next = &pf_norm;
    pf_norm.is_bit_field = FALSE; pf_norm.offset = 4; pf_norm.type = &base_pad_ty; pf_norm.next = &pf_null;
    pf_null.is_bit_field = FALSE; pf_null.offset = 6; pf_null.type = nullptr; pf_null.next = &pf_oob;
    pf_oob.is_bit_field = FALSE; pf_oob.offset = 10; pf_oob.type = &base_pad_ty; pf_oob.next = nullptr;

    a_type padded_cls = {};
    padded_cls.kind = tk_class;
    padded_cls.size = 8; // bytes 0..1 = 0xFF, byte 2 = 0x07 (partial bitfield), byte 3 = 0x00 (padding), bytes 4..5 = 0xFF, bytes 6..7 = 0x00
    padded_cls.variant.class_struct_union.extra_info = &pad_supp;
    padded_cls.variant.class_struct_union.field_list = &pf_bf1;

    a_type ptr_to_padded = {};
    ptr_to_padded.kind = tk_pointer;
    ptr_to_padded.size = 8;
    ptr_to_padded.variant.pointer.type = &padded_cls;
    state.type_cache[&ptr_to_padded] = llvm::PointerType::getUnqual(*state.context);
    state.type_cache[&ptr_type] = llvm::PointerType::getUnqual(*state.context);

    an_expr_node pad_ptr_arg = va_var_node;
    pad_ptr_arg.type = &ptr_to_padded;
    assert(call_test_builtin("__builtin_clear_padding", &int_type, {&pad_ptr_arg}) == llvm_gen_be_error_t::ok);
    pad_ptr_arg.type = &ptr_type;
    ptr_type.variant.pointer.type = nullptr; // pointer to null type fallback
    assert(call_test_builtin("__builtin_clear_padding", &int_type, {&pad_ptr_arg}) == llvm_gen_be_error_t::ok);
    ptr_type.variant.pointer.type = &int_type;
    // Also test non-pointer type and tk_struct without extra_info directly on llvm_compute_type_padding_mask
    assert(llvm_compute_type_padding_mask(&int_type, &pad_mask) == llvm_gen_be_error_t::ok);
    a_type plain_struct_pad = {};
    plain_struct_pad.kind = tk_struct;
    plain_struct_pad.size = 4;
    plain_struct_pad.variant.class_struct_union.extra_info = nullptr;
    assert(llvm_compute_type_padding_mask(&plain_struct_pad, &pad_mask) == llvm_gen_be_error_t::ok);

    // llvm_lower_sizeof_pack
    assert(llvm_lower_sizeof_pack(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node sp_node = {}, sp_elem1 = {}, sp_elem2 = {};
    sp_node.kind = enk_sizeof_pack;
    sp_node.type = &int_type;
    assert(llvm_lower_sizeof_pack(&sp_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    sp_node.next = &sp_elem1;
    sp_elem1.next = &sp_elem2;
    sp_elem2.next = nullptr;
    assert(llvm_lower_expression(&sp_node, &val) == llvm_gen_be_error_t::ok);
    sp_node.next = nullptr;

    // llvm_lower_fold_expression
    assert(llvm_lower_fold_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node fold_node = {}, fold_op1 = {}, fold_op2 = {};
    fold_node.kind = enk_fold;
    fold_node.type = &int_type;
    assert(llvm_lower_fold_expression(&fold_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    fold_op1.kind = enk_variable;
    fold_op1.type = &int_type;
    fold_op1.variant.variable.ptr = &ubsan_var;
    fold_op2 = fold_op1;
    fold_op1.next = &fold_op2;
    fold_op2.next = nullptr;
    fold_node.variant.fold.operands = &fold_op1;

    const a_token_kind fold_tokens[] = {
        tok_plus, tok_plus_assign, tok_minus, tok_minus_assign,
        tok_star, tok_times_assign, tok_divide, tok_divide_assign,
        tok_remainder, tok_remainder_assign, tok_ampersand, tok_and_assign,
        tok_or, tok_or_assign, tok_excl_or, tok_excl_or_assign,
        tok_shift_left, tok_shift_left_assign, tok_shift_right, tok_shift_right_assign,
        tok_eq, tok_ne, tok_lt, tok_le, tok_gt, tok_ge,
        tok_and_and, tok_or_or,
        tok_comma, tok_assign, tok_period_star, tok_arrow_star,
        tok_compl
    };
    for (a_token_kind ftk : fold_tokens) {
      fold_node.variant.fold.operator_token = ftk;
      assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    }
    // Unsigned integer fold for /, %, >>, <, <=, >, >=, and non-tk_integer fold
    int_kind_is_signed[ik_int] = FALSE;
    for (a_token_kind ftk : {tok_divide, tok_remainder, tok_shift_right, tok_lt, tok_le, tok_gt, tok_ge}) {
      fold_node.variant.fold.operator_token = ftk;
      assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    }
    int_kind_is_signed[ik_int] = TRUE;
    fold_op1.type = &enum_int_ty;
    fold_node.variant.fold.operator_token = tok_divide;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);

    // Floating-point fold for +, -, *, /, %, ==, !=, <, <=, >, >=, &&
    fold_op1.type = &float_type;
    fold_op1.variant.variable.ptr = &flt_tbaa_var;
    fold_op2.type = &float_type;
    fold_op2.variant.variable.ptr = &flt_tbaa_var;
    fold_node.type = &float_type;
    for (a_token_kind ftk : {tok_plus, tok_minus, tok_star, tok_divide, tok_remainder,
                             tok_eq, tok_ne, tok_lt, tok_le, tok_gt, tok_ge}) {
      fold_node.variant.fold.operator_token = ftk;
      assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    }
    // Pointer fold for && (non-integer acc type in &&/||!)
    fold_op1.type = &ptr_type;
    fold_op1.variant.variable.ptr = &va_v17;
    fold_op2.type = &ptr_type;
    fold_op2.variant.variable.ptr = &va_v17;
    fold_node.type = &ptr_type;
    fold_node.variant.fold.operator_token = tok_and_and;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    // Error on rhs inside && fold and inside non-logical fold
    fold_op2.variant.variable.ptr = nullptr;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    fold_node.variant.fold.operator_token = tok_plus;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    fold_op2.variant.variable.ptr = &va_v17;

    // Unlowered new and delete via llvm_lower_new_expression / llvm_lower_delete_expression
    assert(llvm_lower_new_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_delete_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node unlow_new = {}, unlow_del = {};
    unlow_new.kind = enk_operation;
    unlow_new.type = &ptr_type;
    assert(llvm_lower_new_expression(&unlow_new, nullptr) == llvm_gen_be_error_t::invalid_argument);
    unlow_new.variant.operation.operands = &va_arg1;
    va_arg1.next = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    va_arg1.variant.variable.ptr = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::unsupported_expr);
    va_arg1.variant.variable.ptr = &ubsan_var;

    unlow_new.variant.operation.operands = nullptr;
    ptr_type.variant.pointer.type = &int_type;
    int_type.size = 4;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    int_type.size = 0;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    int_type.size = 4;
    ptr_type.variant.pointer.type = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    unlow_new.type = &int_type;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    unlow_new.type = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    ptr_type.variant.pointer.type = &int_type;

    unlow_del.kind = enk_operation;
    unlow_del.type = &void_edg_ty;
    assert(llvm_lower_delete_expression(&unlow_del, nullptr) == llvm_gen_be_error_t::invalid_argument);
    unlow_del.variant.operation.operands = nullptr;
    assert(llvm_lower_delete_expression(&unlow_del, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node ptr_del_op = {};
    ptr_del_op.kind = enk_variable;
    ptr_del_op.type = &ptr_type;
    ptr_del_op.variant.variable.ptr = &va_v17;
    unlow_del.variant.operation.operands = &ptr_del_op;
    assert(llvm_lower_delete_expression(&unlow_del, &val) == llvm_gen_be_error_t::ok);
    ptr_del_op.variant.variable.ptr = nullptr;
    assert(llvm_lower_delete_expression(&unlow_del, &val) == llvm_gen_be_error_t::unsupported_expr);

    an_expr_node nd_null_expr = {};
    nd_null_expr.kind = enk_new_delete;
    nd_null_expr.variant.new_delete = nullptr;
    assert(llvm_lower_new_expression(&nd_null_expr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_delete_expression(&nd_null_expr, &val) == llvm_gen_be_error_t::invalid_argument);

    // Phase 1.3: Array new[] and delete[] with over-aligned cookie (alignment == 16) and sized array delete (_ZdaPvm)
    a_type aligned16_cls = {};
    aligned16_cls.kind = tk_class;
    aligned16_cls.size = 777;
    aligned16_cls.alignment = 16;
    state.type_cache[&aligned16_cls] = llvm::StructType::get(*state.context, {llvm::Type::getInt64Ty(*state.context), llvm::Type::getInt64Ty(*state.context)});

    a_routine new_arr_rout = {}, del_arr_sized_rout = {};
    new_arr_rout.source_corresp.name = "_Znam";
    del_arr_sized_rout.source_corresp.name = "_ZdaPvm";

    a_new_delete_supplement nd_arr_aligned = {};
    nd_arr_aligned.routine = &new_arr_rout;
    nd_arr_aligned.type = &aligned16_cls;
    nd_arr_aligned.number_of_elements = &va_arg1;
    va_arg1.next = nullptr;
    an_expr_node new_arr_aligned_expr = {};
    new_arr_aligned_expr.kind = enk_new_delete;
    new_arr_aligned_expr.type = &ptr_type;
    new_arr_aligned_expr.variant.new_delete = &nd_arr_aligned;
    assert(llvm_lower_new_expression(&new_arr_aligned_expr, &val) == llvm_gen_be_error_t::ok);

    ptr_del_op.variant.variable.ptr = &va_v17;
    ptr_del_op.next = nullptr;
    a_new_delete_supplement nd_del_aligned = {};
    nd_del_aligned.routine = &del_arr_sized_rout;
    nd_del_aligned.type = &aligned16_cls;
    nd_del_aligned.array_delete = TRUE;
    nd_del_aligned.arg = &ptr_del_op;
    an_expr_node del_arr_aligned_expr = {};
    del_arr_aligned_expr.kind = enk_new_delete;
    del_arr_aligned_expr.type = &void_edg_ty;
    del_arr_aligned_expr.variant.new_delete = &nd_del_aligned;
    assert(llvm_lower_delete_expression(&del_arr_aligned_expr, &val) == llvm_gen_be_error_t::ok);

    // Also test when get_llvm_type(nd->type, &elem_llvm_ty) fails in array_delete
    a_type bad_elem_cls = {};
    bad_elem_cls.kind = static_cast<a_type_kind>(999);
    bad_elem_cls.size = 777;
    bad_elem_cls.alignment = 8;
    nd_del_aligned.type = &bad_elem_cls;
    assert(llvm_lower_delete_expression(&del_arr_aligned_expr, &val) != llvm_gen_be_error_t::ok);

    // Phase 5: Dynamic atomic fence ordering & C11 _Atomic compound assignments / increment / decrement
    a_variable p5_iv = {}, p5_fv = {};
    p5_iv.type = &int_type;
    p5_fv.type = &float_type;
    state.local_vars[&p5_iv] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    state.local_vars[&p5_fv] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));

    a_routine dyn_fence_r = {};
    dyn_fence_r.source_corresp.name = "__atomic_thread_fence";
    an_expr_node dyn_fence_callee = {};
    dyn_fence_callee.kind = enk_routine;
    dyn_fence_callee.variant.routine.ptr = &dyn_fence_r;
    an_expr_node dyn_fence_arg = {};
    dyn_fence_arg.kind = enk_variable;
    dyn_fence_arg.type = &int_type;
    dyn_fence_arg.variant.variable.ptr = &p5_iv;
    dyn_fence_callee.next = &dyn_fence_arg;
    an_expr_node dyn_fence_call = {};
    dyn_fence_call.kind = enk_operation;
    dyn_fence_call.type = &void_edg_ty;
    dyn_fence_call.variant.operation.kind = eok_call;
    dyn_fence_call.variant.operation.operands = &dyn_fence_callee;
    assert(llvm_lower_expression(&dyn_fence_call, &val) == llvm_gen_be_error_t::ok);

    a_type atomic_int_tref = {};
    atomic_int_tref.kind = tk_typeref;
    atomic_int_tref.variant.typeref.type = &int_type;
    atomic_int_tref.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    a_type atomic_flt_tref = {};
    atomic_flt_tref.kind = tk_typeref;
    atomic_flt_tref.variant.typeref.type = &float_type;
    atomic_flt_tref.variant.typeref.qualifiers = TQ_C11_ATOMIC;

    a_variable atomic_iv = {}, atomic_fv = {};
    atomic_iv.type = &atomic_int_tref;
    atomic_fv.type = &atomic_flt_tref;
    state.local_vars[&atomic_iv] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    state.local_vars[&atomic_fv] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));

    an_expr_node a_ilhs = {}, a_irhs = {}, a_flhs = {}, a_frhs = {};
    a_ilhs.kind = enk_variable; a_ilhs.type = &atomic_int_tref; a_ilhs.variant.variable.ptr = &atomic_iv;
    a_irhs.kind = enk_variable; a_irhs.type = &int_type; a_irhs.variant.variable.ptr = &p5_iv;
    a_flhs.kind = enk_variable; a_flhs.type = &atomic_flt_tref; a_flhs.variant.variable.ptr = &atomic_fv;
    a_frhs.kind = enk_variable; a_frhs.type = &float_type; a_frhs.variant.variable.ptr = &p5_fv;

    an_expr_operator_kind atomic_i_ops[] = {
        eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr,
        eok_add_assign, eok_subtract_assign, eok_and_assign, eok_or_assign, eok_xor_assign, eok_multiply_assign
    };
    for (an_expr_operator_kind ak : atomic_i_ops) {
      an_expr_node a_ce = {};
      a_ce.kind = enk_operation;
      a_ce.type = &int_type;
      a_ce.variant.operation.kind = ak;
      a_ilhs.next = (ak == eok_pre_incr || ak == eok_post_incr || ak == eok_pre_decr || ak == eok_post_decr) ? nullptr : &a_irhs;
      a_ce.variant.operation.operands = &a_ilhs;
      assert(llvm_lower_compound_assignment(&a_ce, &val) == llvm_gen_be_error_t::ok);
    }
    an_expr_operator_kind atomic_f_ops[] = {
        eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr, eok_add_assign, eok_subtract_assign
    };
    for (an_expr_operator_kind ak : atomic_f_ops) {
      an_expr_node a_ce = {};
      a_ce.kind = enk_operation;
      a_ce.type = &float_type;
      a_ce.variant.operation.kind = ak;
      a_flhs.next = (ak == eok_pre_incr || ak == eok_post_incr || ak == eok_pre_decr || ak == eok_post_decr) ? nullptr : &a_frhs;
      a_ce.variant.operation.operands = &a_flhs;
      assert(llvm_lower_compound_assignment(&a_ce, &val) == llvm_gen_be_error_t::ok);
    }

    // Phase 6: Extended complex libcalls (x86_fp80 __mulxc3/__divxc3, fp128 & ppc_fp128 __multc3/__divtc3)
    a_type cplx_fp80_ty = {}, cplx_fp128_ty = {}, cplx_ppcfp128_ty = {};
    cplx_fp80_ty.kind = tk_complex; cplx_fp80_ty.size = 32;
    cplx_fp128_ty.kind = tk_complex; cplx_fp128_ty.size = 32;
    cplx_ppcfp128_ty.kind = tk_complex; cplx_ppcfp128_ty.size = 32;
    llvm::Type* fp80_lt = llvm::Type::getX86_FP80Ty(*state.context);
    llvm::Type* fp128_lt = llvm::Type::getFP128Ty(*state.context);
    llvm::Type* ppc128_lt = llvm::Type::getPPC_FP128Ty(*state.context);
    state.type_cache[&cplx_fp80_ty] = llvm::StructType::get(*state.context, {fp80_lt, fp80_lt});
    state.type_cache[&cplx_fp128_ty] = llvm::StructType::get(*state.context, {fp128_lt, fp128_lt});
    state.type_cache[&cplx_ppcfp128_ty] = llvm::StructType::get(*state.context, {ppc128_lt, ppc128_lt});

    a_type_ptr ext_cplx_tys[3] = {&cplx_fp80_ty, &cplx_fp128_ty, &cplx_ppcfp128_ty};
    for (int ci = 0; ci < 3; ++ci) {
      a_variable cv1 = {}, cv2 = {};
      cv1.type = ext_cplx_tys[ci]; cv2.type = ext_cplx_tys[ci];
      state.local_vars[&cv1] = state.builder->CreateAlloca(state.type_cache[ext_cplx_tys[ci]]);
      state.local_vars[&cv2] = state.builder->CreateAlloca(state.type_cache[ext_cplx_tys[ci]]);
      an_expr_node cop1 = {}, cop2 = {}, cmul = {}, cdiv = {};
      cop1.kind = enk_variable; cop1.type = ext_cplx_tys[ci]; cop1.variant.variable.ptr = &cv1; cop1.next = &cop2;
      cop2.kind = enk_variable; cop2.type = ext_cplx_tys[ci]; cop2.variant.variable.ptr = &cv2;
      cmul.kind = enk_operation; cmul.type = ext_cplx_tys[ci]; cmul.variant.operation.kind = eok_multiply; cmul.variant.operation.operands = &cop1;
      cdiv.kind = enk_operation; cdiv.type = ext_cplx_tys[ci]; cdiv.variant.operation.kind = eok_divide; cdiv.variant.operation.operands = &cop1;
      assert(llvm_lower_arithmetic_expression(&cmul, &val) == llvm_gen_be_error_t::ok);
      assert(llvm_lower_arithmetic_expression(&cdiv, &val) == llvm_gen_be_error_t::ok);
    }

#if FIXED_POINT_ALLOWED
    // Phase 8: Fixed-point arithmetic & casts via llvm_lower_arithmetic_expression and llvm_lower_cast_expression
    a_type fx_sfract = {}, fx_ufract_sat = {}, fx_saccum = {}, fx_unsized = {};
    fx_sfract.kind = tk_fixed_point; fx_sfract.size = 4;
    fx_sfract.variant.fixed_point.is_fract_type = TRUE; fx_sfract.variant.fixed_point.is_unsigned = FALSE; fx_sfract.variant.fixed_point.saturating = FALSE;
    fx_ufract_sat.kind = tk_fixed_point; fx_ufract_sat.size = 4;
    fx_ufract_sat.variant.fixed_point.is_fract_type = TRUE; fx_ufract_sat.variant.fixed_point.is_unsigned = TRUE; fx_ufract_sat.variant.fixed_point.saturating = TRUE;
    fx_saccum.kind = tk_fixed_point; fx_saccum.size = 4;
    fx_saccum.variant.fixed_point.is_fract_type = FALSE; fx_saccum.variant.fixed_point.is_unsigned = FALSE; fx_saccum.variant.fixed_point.saturating = TRUE;
    fx_unsized.kind = tk_fixed_point; fx_unsized.size = 0;
    fx_unsized.variant.fixed_point.is_fract_type = FALSE; fx_unsized.variant.fixed_point.is_unsigned = TRUE; fx_unsized.variant.fixed_point.saturating = FALSE;
    state.type_cache[&fx_sfract] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_ufract_sat] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_saccum] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_unsized] = llvm::Type::getInt32Ty(*state.context);

    an_expr_node fx_op1 = {}, fx_op2 = {}, fx_bin = {};
    fx_op1.kind = enk_variable; fx_op1.type = &fx_sfract; fx_op1.variant.variable.ptr = &p5_iv; fx_op1.next = &fx_op2;
    fx_op2.kind = enk_variable; fx_op2.type = &fx_sfract; fx_op2.variant.variable.ptr = &p5_iv;
    fx_bin.kind = enk_operation; fx_bin.type = &fx_sfract; fx_bin.variant.operation.kind = eok_add; fx_bin.variant.operation.operands = &fx_op1;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);
    fx_bin.type = &fx_ufract_sat; fx_bin.variant.operation.kind = eok_subtract;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);
    fx_bin.type = &fx_saccum; fx_bin.variant.operation.kind = eok_multiply;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);
    fx_bin.type = &fx_unsized; fx_bin.variant.operation.kind = eok_divide;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);

    // Fixed-point <-> floating-point casts (signed and unsigned)
    an_expr_node fx_cast = {};
    fx_cast.kind = enk_operation; fx_cast.variant.operation.kind = eok_cast;
    fx_op1.next = nullptr;
    fx_op1.type = &fx_sfract; fx_cast.type = &float_type; fx_cast.variant.operation.operands = &fx_op1;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_op1.type = &fx_ufract_sat; fx_cast.type = &float_type;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    an_expr_node flt_cast_op = {};
    flt_cast_op.kind = enk_variable; flt_cast_op.type = &float_type; flt_cast_op.variant.variable.ptr = &p5_fv;
    fx_cast.variant.operation.operands = &flt_cast_op;
    fx_cast.type = &fx_sfract;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_cast.type = &fx_ufract_sat;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
#endif

    // Phase 9: UBSan __builtin_unreachable and module source filename resolution in UBSan / __builtin_FILE / __builtin_source_location
    state.module->setSourceFileName("real_unit.cpp");
    state.enable_ubsan_checks = true;
    a_routine unreach_r = {}, bfile_r = {}, bsloc_r = {};
    unreach_r.source_corresp.name = "__builtin_unreachable";
    bfile_r.source_corresp.name = "__builtin_FILE";
    bsloc_r.source_corresp.name = "__builtin_source_location";
    an_expr_node p9_callee = {}, p9_call = {};
    p9_callee.kind = enk_routine;
    p9_call.kind = enk_operation; p9_call.type = &ptr_type; p9_call.variant.operation.kind = eok_call; p9_call.variant.operation.operands = &p9_callee;
    p9_callee.variant.routine.ptr = &bfile_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    p9_callee.variant.routine.ptr = &bsloc_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    p9_callee.variant.routine.ptr = &unreach_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    llvm::BasicBlock* after_unreach_bb = llvm::BasicBlock::Create(
        *state.context, "after.unreach", state.builder->GetInsertBlock()->getParent());
    state.builder->SetInsertPoint(after_unreach_bb);
    state.module->setSourceFileName("");
    state.enable_ubsan_checks = true;
    p9_callee.variant.routine.ptr = &unreach_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    llvm::BasicBlock* after_unreach_bb2 = llvm::BasicBlock::Create(
        *state.context, "after.unreach2", state.builder->GetInsertBlock()->getParent());
    state.builder->SetInsertPoint(after_unreach_bb2);
    state.enable_ubsan_checks = false;
    p9_callee.variant.routine.ptr = &bfile_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    p9_callee.variant.routine.ptr = &bsloc_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);

    // Section 7.2: Extended evaluate_type_trait_builtin and C++17 over-aligned new/delete
    a_type trait_cls_triv = {}, trait_cls_virt = {}, trait_cls_vbase = {}, trait_cls_abs = {};
    trait_cls_triv.kind = tk_class; trait_cls_triv.size = 8;
    trait_cls_virt.kind = tk_struct; trait_cls_virt.size = 8; trait_cls_virt.variant.class_struct_union.any_virtual_functions = TRUE;
    trait_cls_vbase.kind = tk_class; trait_cls_vbase.size = 8; trait_cls_vbase.variant.class_struct_union.any_virtual_base_classes = TRUE;
    trait_cls_abs.kind = tk_class; trait_cls_abs.size = 8; trait_cls_abs.variant.class_struct_union.abstract = TRUE;

    an_expr_node tt_op1 = {}, tt_op2 = {}, tt_expr = {};
    tt_op1.kind = enk_type_operand;
    tt_op2.kind = enk_type_operand;
    tt_expr.kind = enk_builtin_operation;
    tt_expr.type = &int_type;
    tt_expr.variant.builtin_operation.operands = &tt_op1;

    const a_builtin_operation_kind layout_boks[] = {
        bok_is_standard_layout, bok_is_pod, bok_is_literal_type, bok_is_trivially_copyable, bok_is_trivial
    };
    a_type_ptr layout_types[] = {
        &int_type, &float_type, &ptr_type, &trait_cls_triv, &trait_cls_virt, &trait_cls_vbase, &void_edg_ty, nullptr
    };
    for (a_builtin_operation_kind lb : layout_boks) {
      tt_expr.variant.builtin_operation.kind = lb;
      for (a_type_ptr lt : layout_types) {
        tt_op1.variant.type_operand.type = lt;
        assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }

    tt_expr.variant.builtin_operation.kind = bok_has_unique_object_representations;
    for (a_type_ptr ut : {&int_type, &padded_cls, &ptr_type, &void_edg_ty, &zero_sz_struct}) {
      tt_op1.variant.type_operand.type = ut;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
    }

    const a_builtin_operation_kind ctor_boks[] = {
        bok_is_trivially_constructible, bok_is_constructible, bok_is_nothrow_constructible,
        bok_is_destructible, bok_is_nothrow_destructible, bok_is_trivially_destructible
    };
    for (a_builtin_operation_kind cb : ctor_boks) {
      tt_expr.variant.builtin_operation.kind = cb;
      for (a_type_ptr ct : {&int_type, &trait_cls_triv, &trait_cls_abs, &aligned16_cls, &void_edg_ty, &rout_ty}) {
        tt_op1.variant.type_operand.type = ct;
        assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }

    const a_builtin_operation_kind assign_boks[] = {
        bok_is_assignable, bok_is_trivially_assignable, bok_is_nothrow_assignable,
        bok_is_trivially_copy_assignable, bok_is_assignable_no_precondition_check,
        bok_is_convertible_to, bok_is_convertible, bok_is_nothrow_convertible
    };
    tt_op1.next = &tt_op2;
    for (a_builtin_operation_kind ab : assign_boks) {
      tt_expr.variant.builtin_operation.kind = ab;
      tt_op1.variant.type_operand.type = &int_type;
      tt_op2.variant.type_operand.type = &int_type;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      tt_op1.variant.type_operand.type = &aligned16_cls;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      tt_op2.variant.type_operand.type = &void_edg_ty;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      tt_op1.variant.type_operand.type = &void_edg_ty;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
    }
    tt_op1.next = nullptr;

    // C++17 over-aligned new/delete (alignment = 32 > 16)
    a_type aligned32_cls = {};
    aligned32_cls.kind = tk_struct;
    aligned32_cls.size = 777;
    aligned32_cls.alignment = 32;
    state.type_cache[&aligned32_cls] = llvm::StructType::get(
        *state.context, {llvm::Type::getInt64Ty(*state.context), llvm::Type::getInt64Ty(*state.context)});
    a_type ptr_aligned32_ty = {};
    ptr_aligned32_ty.kind = tk_pointer;
    ptr_aligned32_ty.size = 8;
    ptr_aligned32_ty.variant.pointer.type = &aligned32_cls;
    state.type_cache[&ptr_aligned32_ty] = llvm::PointerType::getUnqual(*state.context);

    unlow_new.type = &ptr_aligned32_ty;
    unlow_new.variant.operation.operands = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);

    a_routine new_align_rout = {}, new_plain32_rout = {}, del_align_rout = {}, del_sized_rout = {}, del_arr_align_rout = {};
    new_align_rout.source_corresp.name = "_ZnwmSt11align_val_t";
    new_plain32_rout.source_corresp.name = "_Znwm";
    del_align_rout.source_corresp.name = "_ZdlPvmSt11align_val_t";
    del_sized_rout.source_corresp.name = "_ZdlPvm";
    del_arr_align_rout.source_corresp.name = "_ZdaPvmSt11align_val_t";

    a_new_delete_supplement nd_new32 = {};
    nd_new32.is_new = TRUE;
    nd_new32.routine = &new_align_rout;
    nd_new32.type = &aligned32_cls;
    an_expr_node new32_expr = {};
    new32_expr.kind = enk_new_delete;
    new32_expr.type = &ptr_aligned32_ty;
    new32_expr.variant.new_delete = &nd_new32;
    assert(llvm_lower_new_expression(&new32_expr, &val) == llvm_gen_be_error_t::ok);
    nd_new32.routine = &new_plain32_rout;
    assert(llvm_lower_new_expression(&new32_expr, &val) == llvm_gen_be_error_t::ok);

    ptr_del_op.variant.variable.ptr = &va_v17;
    ptr_del_op.next = nullptr;
    a_new_delete_supplement nd_del32 = {};
    nd_del32.routine = &del_align_rout;
    nd_del32.type = &aligned32_cls;
    nd_del32.arg = &ptr_del_op;
    an_expr_node del32_expr = {};
    del32_expr.kind = enk_new_delete;
    del32_expr.type = &void_edg_ty;
    del32_expr.variant.new_delete = &nd_del32;
    assert(llvm_lower_delete_expression(&del32_expr, &val) == llvm_gen_be_error_t::ok);

    nd_del32.routine = &del_sized_rout;
    assert(llvm_lower_delete_expression(&del32_expr, &val) == llvm_gen_be_error_t::ok);

    nd_del32.routine = &del_arr_align_rout;
    nd_del32.array_delete = TRUE;
    assert(llvm_lower_delete_expression(&del32_expr, &val) == llvm_gen_be_error_t::ok);

    // Phase 5: UBSan TypeDescriptor, DebugLoc SourceLocation, invalid-bool load, and float-cast-overflow
    llvm::GlobalVariable* ubsan_td = nullptr;
    assert(llvm_emit_ubsan_type_descriptor(&int_type, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_ubsan_type_descriptor(nullptr, &ubsan_td) == llvm_gen_be_error_t::ok);
    assert(ubsan_td != nullptr);
    int_type.source_corresp.name = (char*)"int";
    int_type.size = 4;
    assert(llvm_emit_ubsan_type_descriptor(&int_type, &ubsan_td) == llvm_gen_be_error_t::ok);
    int_type.size = 0;
    int_type.source_corresp.name = nullptr;
    assert(llvm_emit_ubsan_type_descriptor(&int_type, &ubsan_td) == llvm_gen_be_error_t::ok);
    int_type.size = 4;

    a_type flt_td_ty = {};
    flt_td_ty.kind = tk_float;
    flt_td_ty.size = 4;
    assert(llvm_emit_ubsan_type_descriptor(&flt_td_ty, &ubsan_td) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_ubsan_type_descriptor(&aligned32_cls, &ubsan_td) == llvm_gen_be_error_t::ok);

    // Test UBSan check with active DebugLoc, invalid bool load, and float-to-int cast
    llvm::DIBuilder dib(*state.module);
    llvm::DIFile* dif = dib.createFile("ubsan_test.cpp", ".");
    llvm::DISubprogram* disp = dib.createFunction(
        dif, "ubsan_fn", "ubsan_fn", dif, 10, dib.createSubroutineType(dib.getOrCreateTypeArray({})), 10,
        llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagDefinition);
    state.builder->SetCurrentDebugLocation(llvm::DILocation::get(*state.context, 42, 7, disp));
    state.enable_ubsan_checks = true;

    a_type bool_load_ty = {};
    bool_load_ty.kind = tk_integer;
    bool_load_ty.variant.integer.bool_type = TRUE;
    bool_load_ty.size = 1;
    state.type_cache[&bool_load_ty] = llvm::Type::getInt8Ty(*state.context);
    a_variable bool_var = {};
    bool_var.type = &bool_load_ty;
    state.local_vars[&bool_var] = state.builder->CreateAlloca(llvm::Type::getInt8Ty(*state.context));
    an_expr_node bool_var_expr = {};
    bool_var_expr.kind = enk_variable;
    bool_var_expr.type = &bool_load_ty;
    bool_var_expr.variant.variable.ptr = &bool_var;
    assert(llvm_lower_expression(&bool_var_expr, &val) == llvm_gen_be_error_t::ok);

    // Also test is_bool_type == TRUE with non-i8 type (e.g. i32) to cover false branch of isIntegerTy(8)
    state.type_cache[&bool_load_ty] = llvm::Type::getInt32Ty(*state.context);
    state.local_vars[&bool_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    assert(llvm_lower_expression(&bool_var_expr, &val) == llvm_gen_be_error_t::ok);

    state.type_cache[&flt_td_ty] = llvm::Type::getFloatTy(*state.context);
    a_variable flt_var = {};
    flt_var.type = &flt_td_ty;
    state.local_vars[&flt_var] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));
    an_expr_node flt_op = {};
    flt_op.kind = enk_variable;
    flt_op.type = &flt_td_ty;
    flt_op.variant.variable.ptr = &flt_var;
    an_expr_node fp_cast_expr = {};
    fp_cast_expr.kind = enk_operation;
    fp_cast_expr.type = &int_type;
    fp_cast_expr.variant.operation.kind = eok_cast;
    fp_cast_expr.variant.operation.operands = &flt_op;
    assert(llvm_lower_cast_expression(&fp_cast_expr, &val) == llvm_gen_be_error_t::ok);

    state.enable_ubsan_checks = false;
    state.builder->SetCurrentDebugLocation(llvm::DebugLoc());

    // Phase 9: Test addrspacecast in llvm_lower_cast_expression
    a_type ptr_as0_ty = {};
    ptr_as0_ty.kind = tk_pointer;
    a_type ptr_as3_ty = {};
    ptr_as3_ty.kind = tk_pointer;
    state.type_cache[&ptr_as0_ty] = llvm::PointerType::get(*state.context, 0);
    state.type_cache[&ptr_as3_ty] = llvm::PointerType::get(*state.context, 3);
    a_variable ptr_as0_var = {};
    ptr_as0_var.type = &ptr_as0_ty;
    state.local_vars[&ptr_as0_var] = state.builder->CreateAlloca(llvm::PointerType::get(*state.context, 0));
    an_expr_node ptr_as0_op = {};
    ptr_as0_op.kind = enk_variable;
    ptr_as0_op.type = &ptr_as0_ty;
    ptr_as0_op.variant.variable.ptr = &ptr_as0_var;
    an_expr_node as_cast_expr = {};
    as_cast_expr.kind = enk_operation;
    as_cast_expr.type = &ptr_as3_ty;
    as_cast_expr.variant.operation.kind = eok_cast;
    as_cast_expr.variant.operation.operands = &ptr_as0_op;
    assert(llvm_lower_cast_expression(&as_cast_expr, &val) == llvm_gen_be_error_t::ok);

#if FIXED_POINT_ALLOWED
    // Workstream 9: Test fixed-point arithmetic (add, subtract, multiply, divide, saturating & non-saturating, signed & unsigned) and conversions
    a_type fx_s16 = {}, fx_u16 = {}, fx_s32 = {}, fx_u32 = {};
    fx_s16.kind = tk_fixed_point; fx_s16.size = 2; fx_s16.variant.fixed_point.precision = fpp_short; fx_s16.variant.fixed_point.is_unsigned = FALSE; fx_s16.variant.fixed_point.saturating = FALSE;
    fx_u16.kind = tk_fixed_point; fx_u16.size = 2; fx_u16.variant.fixed_point.precision = fpp_short; fx_u16.variant.fixed_point.is_unsigned = TRUE; fx_u16.variant.fixed_point.saturating = TRUE;
    fx_s32.kind = tk_fixed_point; fx_s32.size = 4; fx_s32.variant.fixed_point.precision = fpp_default; fx_s32.variant.fixed_point.is_unsigned = FALSE; fx_s32.variant.fixed_point.saturating = TRUE;
    fx_u32.kind = tk_fixed_point; fx_u32.size = 4; fx_u32.variant.fixed_point.precision = fpp_long; fx_u32.variant.fixed_point.is_unsigned = TRUE; fx_u32.variant.fixed_point.saturating = FALSE;

    state.type_cache[&fx_s16] = llvm::Type::getInt16Ty(*state.context);
    state.type_cache[&fx_u16] = llvm::Type::getInt16Ty(*state.context);
    state.type_cache[&fx_s32] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_u32] = llvm::Type::getInt32Ty(*state.context);

    a_variable fx_v16 = {}, fx_v32 = {};
    fx_v16.type = &fx_s16;
    fx_v32.type = &fx_s32;
    state.local_vars[&fx_v16] = state.builder->CreateAlloca(llvm::Type::getInt16Ty(*state.context));
    state.local_vars[&fx_v32] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));

    an_expr_node fx_op1 = {}, fx_op2 = {};
    fx_op1.kind = enk_variable; fx_op1.type = &fx_s32; fx_op1.variant.variable.ptr = &fx_v32; fx_op1.next = &fx_op2;
    fx_op2.kind = enk_variable; fx_op2.type = &fx_s32; fx_op2.variant.variable.ptr = &fx_v32; fx_op2.next = nullptr;

    an_expr_node fx_arith = {};
    fx_arith.kind = enk_operation;
    fx_arith.variant.operation.operands = &fx_op1;
    const an_expr_operator_kind fx_ops[] = {eok_add, eok_subtract, eok_multiply, eok_divide, eok_and};
    a_type* fx_tys[] = {&fx_s16, &fx_u16, &fx_s32, &fx_u32};
    for (a_type* fxt : fx_tys) {
      fx_arith.type = fxt;
      for (an_expr_operator_kind ok : fx_ops) {
        fx_arith.variant.operation.kind = ok;
        assert(llvm_lower_arithmetic_expression(&fx_arith, &val) == llvm_gen_be_error_t::ok);
      }
    }
    // Also test unary negate on tk_fixed_point (v2 == nullptr)
    fx_op1.next = nullptr;
    fx_arith.type = &fx_s32;
    fx_arith.variant.operation.kind = eok_negate;
    assert(llvm_lower_arithmetic_expression(&fx_arith, &val) == llvm_gen_be_error_t::ok);

    // Fixed-point conversions: fx->fx (dst_f > src_f, src_f > dst_f signed & unsigned, dst_f == src_f), fx->fp (signed & unsigned), fp->fx (signed & unsigned), fx->int (signed & unsigned), int->fx (signed & unsigned)
    an_expr_node fx_cast = {};
    fx_cast.kind = enk_operation;
    fx_cast.variant.operation.kind = eok_cast;
    fx_cast.variant.operation.operands = &fx_op1;

    // 1. fx_s16 (f=7) -> fx_s32 (f=15) [dst_f > src_f]
    fx_op1.type = &fx_s16; fx_op1.variant.variable.ptr = &fx_v16;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 2. fx_s32 (f=15, signed) -> fx_s16 (f=7) [src_f > dst_f, signed]
    fx_op1.type = &fx_s32; fx_op1.variant.variable.ptr = &fx_v32;
    fx_cast.type = &fx_s16;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 3. fx_u32 (f=32, unsigned) -> fx_s16 (f=7) [src_f > dst_f, unsigned]
    fx_op1.type = &fx_u32; fx_op1.variant.variable.ptr = &fx_v32;
    fx_cast.type = &fx_s16;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 4. fx_s32 -> fx_s32 [dst_f == src_f]
    fx_op1.type = &fx_s32; fx_op1.variant.variable.ptr = &fx_v32;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 5. fx->fp (signed & unsigned)
    fx_op1.type = &fx_s32; fx_cast.type = &flt_td_ty;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_op1.type = &fx_u32; fx_cast.type = &flt_td_ty;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 6. fp->fx (signed & unsigned)
    fx_cast.variant.operation.operands = &flt_op;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_cast.type = &fx_u32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 7. fx->int (signed & unsigned)
    fx_cast.variant.operation.operands = &fx_op1;
    fx_op1.type = &fx_s32; fx_cast.type = &int_type;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_op1.type = &fx_u32; fx_cast.type = &int_type;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 8. int->fx
    a_variable int_fx_var = {};
    int_fx_var.type = &int_type;
    state.local_vars[&int_fx_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    an_expr_node int_fx_op = {};
    int_fx_op.kind = enk_variable; int_fx_op.type = &int_type; int_fx_op.variant.variable.ptr = &int_fx_var;
    fx_cast.variant.operation.operands = &int_fx_op;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
#endif
  }

  printf("All tests passed!\n");
  return 0;
}
namespace edg {
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_expressions.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    if (stmt && stmt->kind == stmk_return) return llvm_gen_be_error_t::unsupported_stmt;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `llvm_emit_dynamic_init`.
   * @details Provides mock or helper behavior for `llvm_emit_dynamic_init` within `tests/test_llvm_expressions.cpp`.
   * @param[in] dip Parameter `dip` for `llvm_emit_dynamic_init`.
   * @param[in] ptr Parameter `ptr` for `llvm_emit_dynamic_init`.
   * @param[in] obj_type Parameter `obj_type` for `llvm_emit_dynamic_init`.
   * @return Result of `llvm_emit_dynamic_init` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_emit_dynamic_init(a_dynamic_init_ptr dip, llvm::Value* ptr, llvm::Type* obj_type) noexcept {
    if (!dip || !ptr) return llvm_gen_be_error_t::invalid_argument;
    if (dip->kind == dik_none || (dip->variable && dip->kind != dik_constant)) return llvm_gen_be_error_t::unsupported_expr;
    if (dip->kind == dik_expression && dip->variant.expression) {
      llvm::Value* init_val = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(dip->variant.expression, &init_val);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (init_val) {
        be_state->builder->CreateStore(init_val, ptr);
        return llvm_gen_be_error_t::ok;
      }
    }
    if (obj_type && obj_type->isSized()) be_state->builder->CreateStore(llvm::Constant::getNullValue(obj_type), ptr);
    return llvm_gen_be_error_t::ok;
  }
}
