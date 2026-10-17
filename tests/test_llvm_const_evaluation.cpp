/**
 * @file test_llvm_const_evaluation.cpp
 * @brief Unit tests for the LLVM backend constant evaluator subsystem.
 * @details Validates integer, floating-point, string, address, and struct/array/union aggregate constants.
 */

#include "llvm_gen_be_const.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_error.h"
#include "il.h"
#include "il_def.h"
#include "target.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_float = 4;
  a_targ_size_t targ_sizeof_double = 8;
  a_targ_size_t targ_sizeof_long_double = 16;
  a_targ_size_t targ_sizeof_float80 = 10;
  a_targ_size_t targ_sizeof_float128 = 16;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_byte_boolean int_kind_is_signed[ik_last]; // mock

  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTI_mock"; }
  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (out_val) *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `fp_to_string`.
   * @details Provides mock or helper behavior for `fp_to_string` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] fk Parameter `fk` for `fp_to_string`.
   * @param[in] val Parameter `val` for `fp_to_string`.
   * @param[in] pos_inf Parameter `pos_inf` for `fp_to_string`.
   * @param[in] neg_inf Parameter `neg_inf` for `fp_to_string`.
   * @param[in] nan Parameter `nan` for `fp_to_string`.
   * @return Result of `fp_to_string` (a_number_buffer).
   */
  a_number_buffer fp_to_string(a_float_kind fk, an_internal_float_value * val, a_boolean* pos_inf, a_boolean* neg_inf, a_boolean* nan) {
    if (val->bytes[0] == 1) *pos_inf = TRUE;
    else if (val->bytes[0] == 2) *neg_inf = TRUE;
    else if (val->bytes[0] == 3) *nan = TRUE;
    else return a_number_buffer("3.14");
    return a_number_buffer("");
  }
}

/**
 * @brief Entry point for the constant evaluation unit test suite.
 * @details Exercises all `llvm_const_from_*` and `evaluate_constant` code paths.
 * @return 0 on success.
 */
int main(void) {
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_mod", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* func = llvm::Function::Create(fty, llvm::GlobalValue::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", func);
  state.builder->SetInsertPoint(bb);

  a_type dummy_int_ty;
  memset(&dummy_int_ty, 0, sizeof(dummy_int_ty));
  dummy_int_ty.kind = tk_integer;
  dummy_int_ty.variant.integer.int_kind = ik_int;

  a_constant dummy_const;
  memset(&dummy_const, 0, sizeof(dummy_const));

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::Constant* out = nullptr;

  dummy_const.kind = ck_integer;
  dummy_const.type = &dummy_int_ty;
  dummy_const.variant.integer_value = 42;
  
  assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantInt>(out)->getZExtValue() == 42);

  fprintf(stderr, "Test integer passed\n");

  // Test evaluate_constant
  assert(evaluate_constant(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantInt>(out)->getZExtValue() == 42);

  dummy_const.kind = ck_float;
  a_type dummy_fp_ty;
  memset(&dummy_fp_ty, 0, sizeof(dummy_fp_ty));
  dummy_fp_ty.kind = tk_float;
  dummy_fp_ty.variant.float_kind = fk_float;
  dummy_const.type = &dummy_fp_ty;
  dummy_const.variant.float_value.bytes[0] = 0; // fallback to 3.14
  llvm::Type* f32_ty = llvm::Type::getFloatTy(*state.context);
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  
  dummy_const.variant.float_value.bytes[0] = 1; // pos inf
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantFP>(out)->isInfinity());
  
  dummy_const.variant.float_value.bytes[0] = 2; // neg inf
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantFP>(out)->isInfinity());

  dummy_const.variant.float_value.bytes[0] = 3; // nan
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantFP>(out)->isNaN());
  
  fprintf(stderr, "Test float passed\n");

  // Test address
  a_label main_lbl_1 = {}, main_lbl_2 = {};
  dummy_const.kind = ck_address;
  dummy_const.variant.address.kind = abk_label;
  dummy_const.variant.address.variant.label = &main_lbl_1;
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*state.context);
  assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  
  dummy_const.variant.address.kind = static_cast<an_address_base_kind>(99);
  assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->isNullValue());

  fprintf(stderr, "Test address passed\n");

  // Test string
  dummy_const.kind = ck_string;
  a_type dummy_arr_ty;
  memset(&dummy_arr_ty, 0, sizeof(dummy_arr_ty));
  dummy_arr_ty.kind = tk_array;
  dummy_arr_ty.variant.array.element_type = &dummy_int_ty;
  dummy_arr_ty.variant.array.variant.number_of_elements = 4;
  dummy_const.type = &dummy_arr_ty;
  dummy_const.variant.string.length = 4;
  dummy_const.variant.string.value = "test";
  assert(llvm_const_from_string(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::isa<llvm::GlobalVariable>(out));
  
  fprintf(stderr, "Test string passed\n");

  // Test aggregate
  dummy_const.kind = ck_aggregate;
  a_constant elem1, elem2;
  memset(&elem1, 0, sizeof(elem1));
  memset(&elem2, 0, sizeof(elem2));
  elem1.kind = ck_integer;
  elem1.type = &dummy_int_ty;
  elem1.variant.integer_value = 1;
  elem1.next = &elem2;
  elem2.kind = ck_integer;
  elem2.type = &dummy_int_ty;
  elem2.variant.integer_value = 2;
  dummy_const.variant.aggregate.first_constant = &elem1;
  
  llvm::ArrayType* arr_ty = llvm::ArrayType::get(i32_ty, 3);
  assert(llvm_const_from_aggregate(&dummy_const, arr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == arr_ty);

  llvm::FixedVectorType* vec_ty = llvm::FixedVectorType::get(i32_ty, 4);
  assert(llvm_const_from_aggregate(&dummy_const, vec_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == vec_ty);
  a_constant bad_vec_elem = {};
  bad_vec_elem.kind = static_cast<a_constant_repr_kind>(99);
  dummy_const.variant.aggregate.first_constant = &bad_vec_elem;
  assert(llvm_const_from_aggregate(&dummy_const, vec_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
  dummy_const.variant.aggregate.first_constant = &elem1;
  
  llvm::StructType* st_ty = llvm::StructType::get(*state.context, {i32_ty, i32_ty, i32_ty});
  assert(llvm_const_from_aggregate(&dummy_const, st_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == st_ty);
  
  // Test Union Aggregate
  a_type dummy_union_ty;
  memset(&dummy_union_ty, 0, sizeof(dummy_union_ty));
  dummy_union_ty.kind = tk_union;
  dummy_const.type = &dummy_union_ty;
  
  llvm::Type* un_elems[] = {llvm::Type::getDoubleTy(*state.context)};
  llvm::StructType* un_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(un_elems));
  assert(llvm_const_from_aggregate(&dummy_const, un_ty, &out) == llvm_gen_be_error_t::ok);

  // Test Union Aggregate with smaller integer member coerced into i64 or [8 x i8]
  llvm::Type* un_i64_elems[] = {llvm::Type::getInt64Ty(*state.context)};
  llvm::StructType* un_i64_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(un_i64_elems));
  assert(llvm_const_from_aggregate(&dummy_const, un_i64_ty, &out) == llvm_gen_be_error_t::ok);

  llvm::Type* un_bytes_elems[] = {llvm::ArrayType::get(llvm::Type::getInt8Ty(*state.context), 8)};
  llvm::StructType* un_bytes_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(un_bytes_elems));
  assert(llvm_const_from_aggregate(&dummy_const, un_bytes_ty, &out) == llvm_gen_be_error_t::ok);
  out->print(llvm::errs()); llvm::errs() << "\n";

  // Test Struct Aggregate with Bitfields and Synthetic Padding
  a_field bf1 = {};
  bf1.is_bit_field = TRUE;
  bf1.offset = 0;
  bf1.offset_bit_remainder = 0;
  bf1.bit_size = 3;
  bf1.type = &dummy_int_ty;
  a_field bf2 = {};
  bf2.is_bit_field = TRUE;
  bf2.offset = 0;
  bf2.offset_bit_remainder = 3;
  bf2.bit_size = 5;
  bf2.type = &dummy_int_ty;
  a_field normal_f = {};
  normal_f.is_bit_field = FALSE;
  normal_f.offset = 4;
  normal_f.type = &dummy_int_ty;
  bf1.next = &bf2;
  bf2.next = &normal_f;

  a_type bf_struct_ty = {};
  bf_struct_ty.kind = tk_struct;
  bf_struct_ty.variant.class_struct_union.field_list = &bf1;
  dummy_const.type = &bf_struct_ty;

  a_constant bf_c1 = {};
  bf_c1.kind = ck_integer;
  bf_c1.type = &dummy_int_ty;
  bf_c1.variant.integer_value = 5;
  a_constant bf_c2 = {};
  bf_c2.kind = ck_integer;
  bf_c2.type = &dummy_int_ty;
  bf_c2.variant.integer_value = 3;
  a_constant bf_c3 = {};
  bf_c3.kind = ck_integer;
  bf_c3.type = &dummy_int_ty;
  bf_c3.variant.integer_value = 99;
  bf_c1.next = &bf_c2;
  bf_c2.next = &bf_c3;
  dummy_const.variant.aggregate.first_constant = &bf_c1;

  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*state.context);
  llvm::ArrayType* pad3_ty = llvm::ArrayType::get(i8_ty, 3);
  llvm::StructType* bf_llvm_st = llvm::StructType::get(*state.context, {i8_ty, pad3_ty, i32_ty});
  assert(llvm_const_from_aggregate(&dummy_const, bf_llvm_st, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == bf_llvm_st);
  
  fprintf(stderr, "Test aggregate passed\n");

  // Test nulls and invalid args
  assert(llvm_const_from_integer(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_integer(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_float(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_float(&dummy_const, f32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_string(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_string(&dummy_const, ptr_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_aggregate(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_aggregate(&dummy_const, st_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_address(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_address(&dummy_const, ptr_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(evaluate_constant(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  
  assert(evaluate_constant(nullptr, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->isNullValue());
  
  dummy_const.kind = static_cast<a_constant_repr_kind>(99);
  assert(evaluate_constant(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

  // Additional branch coverage tests for integer constants (signed, 128-bit, tk_enum, non-int kind)
  {
    int_kind_is_signed[ik_int] = TRUE;
    dummy_const.kind = ck_integer;
    dummy_const.type = &dummy_int_ty;
    dummy_const.variant.integer_value = -7;
    assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_integer(&dummy_const, llvm::Type::getInt128Ty(*state.context), &out) == llvm_gen_be_error_t::ok);

    a_type enum_ty = {};
    enum_ty.kind = tk_enum;
    enum_ty.variant.integer.int_kind = ik_int;
    dummy_const.type = &enum_ty;
    assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);

    dummy_const.type = &dummy_fp_ty; // neither tk_integer nor tk_enum
    assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
  }

  // Additional branch coverage tests for string constants (i8, i16, i32, cache hit, non-pointer expected_ty, non-array type, error)
  {
    a_type char_ty = {}, char16_ty = {}, err_ty = {};
    char_ty.kind = tk_integer;
    char_ty.variant.integer.int_kind = ik_char;
    char16_ty.kind = tk_integer;
    char16_ty.variant.integer.int_kind = ik_unsigned_short;
    err_ty.kind = static_cast<a_type_kind>(999);

    a_type str8_arr_ty = {}, str16_arr_ty = {}, non_arr_ty = {};
    str8_arr_ty.kind = tk_array;
    str8_arr_ty.variant.array.element_type = &char_ty;
    str8_arr_ty.variant.array.variant.number_of_elements = 4;

    str16_arr_ty.kind = tk_array;
    str16_arr_ty.variant.array.element_type = &char16_ty;
    str16_arr_ty.variant.array.variant.number_of_elements = 2;

    non_arr_ty.kind = tk_pointer;

    dummy_const.kind = ck_string;
    dummy_const.variant.string.length = 4;
    dummy_const.variant.string.value = "test"; // hits string_literal_cache from earlier test!
    dummy_const.type = &str8_arr_ty;
    assert(evaluate_constant(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // Non-pointer expected_ty returns ConstantDataArray directly
    llvm::Type* i8_arr4_ty = llvm::ArrayType::get(i8_ty, 4);
    assert(llvm_const_from_string(&dummy_const, i8_arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // UTF-16 string
    dummy_const.type = &str16_arr_ty;
    assert(llvm_const_from_string(&dummy_const, i8_arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // Non-array con->type recovers gracefully as i8
    dummy_const.type = &non_arr_ty;
    assert(llvm_const_from_string(&dummy_const, i8_arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // Error in con->type
    dummy_const.type = &err_ty;
    assert(llvm_const_from_string(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);
  }

  // Additional branch coverage tests for address constants (abk_label cached & unparented, abk_variable, abk_routine, abk_constant, offset != 0)
  {
    dummy_const.kind = ck_address;
    dummy_const.variant.address.offset = 8;
    // Cached label hit
    dummy_const.variant.address.kind = abk_label;
    dummy_const.variant.address.variant.label = &main_lbl_1;
    assert(evaluate_constant(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // Unparented label_bb in label_blocks
    a_label_ptr unparented_lbl = &main_lbl_2;
    llvm::BasicBlock* unparented_bb = llvm::BasicBlock::Create(*state.context, "unparented_lbl");
    llvm::ReturnInst::Create(*state.context, nullptr, unparented_bb);
    state.label_blocks[unparented_lbl] = unparented_bb;
    dummy_const.variant.address.variant.label = unparented_lbl;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    dummy_const.variant.address.offset = 0;

    // abk_variable: null var, null name, invalid type, new global, existing global
    dummy_const.variant.address.kind = abk_variable;
    dummy_const.variant.address.variant.variable = nullptr;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_variable addr_var = {};
    dummy_const.variant.address.variant.variable = &addr_var;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type bad_ty = {};
    bad_ty.kind = static_cast<a_type_kind>(999);
    addr_var.source_corresp.name = (char*)"addr_gvar";
    addr_var.type = &bad_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    addr_var.type = &dummy_int_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Second call hits existing GlobalVariable in module
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_routine: null routine, null name, invalid type, new function, existing function
    dummy_const.variant.address.kind = abk_routine;
    dummy_const.variant.address.variant.routine = nullptr;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_routine addr_rout = {};
    dummy_const.variant.address.variant.routine = &addr_rout;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type rout_ty = {};
    rout_ty.kind = tk_routine;
    rout_ty.variant.routine.return_type = &bad_ty;
    addr_rout.source_corresp.name = (char*)"addr_fn";
    addr_rout.type = &rout_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    rout_ty.variant.routine.return_type = &dummy_int_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Second call hits existing Function in module
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_constant: invalid target->type, invalid target constant, valid target constant
    dummy_const.variant.address.kind = abk_constant;
    a_constant target_con = {};
    target_con.kind = static_cast<a_constant_repr_kind>(99);
    target_con.type = &bad_ty;
    dummy_const.variant.address.variant.constant = &target_con;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    target_con.type = &dummy_int_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    target_con.kind = ck_string;
    target_con.variant.string.length = 2;
    target_con.variant.string.value = "x";
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  }

  // Additional branch coverage for evaluate_constant(ck_float, ck_aggregate, ck_ptr_to_member) and aggregate edge cases
  {
    dummy_const.kind = ck_float;
    dummy_const.type = &dummy_fp_ty;
    dummy_const.variant.float_value.bytes[0] = 0;
    assert(evaluate_constant(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);

    // ck_ptr_to_member via evaluate_constant
    a_type p2m_ty = {};
    p2m_ty.kind = tk_ptr_to_member;
    p2m_ty.size = 8;
    p2m_ty.variant.ptr_to_member.type = &dummy_int_ty;
    a_field p2m_field = {};
    p2m_field.offset = 4;
    a_constant p2m_con = {};
    p2m_con.kind = ck_ptr_to_member;
    p2m_con.type = &p2m_ty;
    p2m_con.variant.ptr_to_member.is_function_ptr = FALSE;
    p2m_con.variant.ptr_to_member.variant.field = &p2m_field;
    assert(evaluate_constant(&p2m_con, llvm::Type::getInt64Ty(*state.context), &out) == llvm_gen_be_error_t::ok);

    // Aggregate with non-array, non-struct expected_ty
    a_constant agg_con = {};
    agg_con.kind = ck_aggregate;
    agg_con.type = &dummy_int_ty;
    assert(evaluate_constant(&agg_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // Array aggregate with invalid element constant
    a_constant bad_elem = {};
    bad_elem.kind = static_cast<a_constant_repr_kind>(99);
    bad_elem.type = &dummy_int_ty;
    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, arr_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Union aggregate edge cases:
    // 1. c == nullptr and empty struct {}, plus c != nullptr with empty struct {}
    a_type un_edg_ty = {};
    un_edg_ty.kind = tk_union;
    agg_con.type = &un_edg_ty;
    agg_con.variant.aggregate.first_constant = nullptr;
    llvm::StructType* empty_st_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{});
    assert(llvm_const_from_aggregate(&agg_con, empty_st_ty, &out) == llvm_gen_be_error_t::ok);
    agg_con.variant.aggregate.first_constant = &elem1;
    assert(llvm_const_from_aggregate(&agg_con, empty_st_ty, &out) == llvm_gen_be_error_t::ok);

    // 2. Union with multi-element StructType { i32, [4 x i8] } and exact type match (target_ty == elem0_ty)
    llvm::StructType* multi_un_st = llvm::StructType::get(*state.context, {i32_ty, llvm::ArrayType::get(i8_ty, 4)});
    agg_con.variant.aggregate.first_constant = &elem1;
    elem1.next = nullptr;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::ok);

    // 3. Union where c->type is invalid, and where c is invalid
    a_type bad_ty = {};
    bad_ty.kind = static_cast<a_type_kind>(999);
    a_constant un_bad_type_c = {};
    un_bad_type_c.kind = ck_integer;
    un_bad_type_c.type = &bad_ty;
    agg_con.variant.aggregate.first_constant = &un_bad_type_c;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::ok);

    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::ok);

    // 4. Union where active_val is float (non-integer) and elem0_ty is [8 x i8] or [2 x i32]
    a_constant fp_c = {};
    fp_c.kind = ck_float;
    fp_c.type = &dummy_fp_ty;
    agg_con.variant.aggregate.first_constant = &fp_c;
    assert(llvm_const_from_aggregate(&agg_con, un_bytes_ty, &out) == llvm_gen_be_error_t::ok);
    llvm::Type* i32_arr2_elem[] = {llvm::ArrayType::get(i32_ty, 2)};
    llvm::StructType* un_i32arr_st = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(i32_arr2_elem));
    assert(llvm_const_from_aggregate(&agg_con, un_i32arr_st, &out) == llvm_gen_be_error_t::ok);

    // 5. Union where elem0_ty is [2 x i8] (smaller than 4-byte i32 active_val -> b < num_bytes stops loop)
    agg_con.variant.aggregate.first_constant = &elem1;
    llvm::Type* i8_arr2_elem[] = {llvm::ArrayType::get(i8_ty, 2)};
    llvm::StructType* un_2bytes_st = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(i8_arr2_elem));
    assert(llvm_const_from_aggregate(&agg_con, un_2bytes_st, &out) == llvm_gen_be_error_t::ok);

    // Struct/class aggregate edge cases:
    // 1. tk_class with real tk_array field, non-i8 array field, bitfield overflow (bit_off + bit_sz > unit_bits),
    //    bit_sz == 0, non-integer bitfield constant, c == nullptr with remaining field, and field == nullptr with trailing pad
    a_type cls_agg_ty = {};
    cls_agg_ty.kind = tk_class;
    a_type i8_arr_edg_ty = {};
    i8_arr_edg_ty.kind = tk_array;
    a_field cf_arr = {}, cf_bf1 = {}, cf_bf0 = {}, cf_bf_over = {}, cf_extra = {};
    cf_arr.type = &i8_arr_edg_ty;
    cf_arr.next = &cf_bf1;

    cf_bf1.is_bit_field = TRUE;
    cf_bf1.offset = 4;
    cf_bf1.offset_bit_remainder = 0;
    cf_bf1.bit_size = 4;
    cf_bf1.type = &dummy_int_ty;
    cf_bf1.next = &cf_bf0;

    cf_bf0.is_bit_field = TRUE;
    cf_bf0.offset = 4;
    cf_bf0.offset_bit_remainder = 4;
    cf_bf0.bit_size = 0; // bit_sz == 0
    cf_bf0.type = &dummy_int_ty;
    cf_bf0.next = &cf_bf_over;

    cf_bf_over.is_bit_field = TRUE;
    cf_bf_over.offset = 5; // bit_off = 8, bit_sz = 4 -> 12 > 8 (unit_bits), breaks!
    cf_bf_over.offset_bit_remainder = 0;
    cf_bf_over.bit_size = 4;
    cf_bf_over.type = &dummy_int_ty;
    cf_bf_over.next = &cf_extra;

    cf_extra.is_bit_field = FALSE;
    cf_extra.type = &dummy_int_ty;
    cf_extra.next = nullptr;

    cls_agg_ty.variant.class_struct_union.field_list = &cf_arr;
    agg_con.type = &cls_agg_ty;

    llvm::ArrayType* pad2_ty = llvm::ArrayType::get(i8_ty, 2);
    llvm::ArrayType* arr_i32_1 = llvm::ArrayType::get(i32_ty, 1);
    llvm::StructType* complex_cls_st = llvm::StructType::get(
        *state.context, {pad2_ty, i8_ty, i8_ty, arr_i32_1, pad2_ty, i32_ty});

    a_constant cc_arr = {}, cc_bf1 = {}, cc_bf0_c = {};
    cc_arr.kind = ck_aggregate;
    cc_arr.type = &i8_arr_edg_ty;
    cc_arr.next = &cc_bf1;
    cc_bf1.kind = ck_float; // c->kind != ck_integer
    cc_bf1.type = &dummy_fp_ty;
    cc_bf1.next = &cc_bf0_c;
    cc_bf0_c.kind = ck_integer;
    cc_bf0_c.type = &dummy_int_ty;
    cc_bf0_c.next = nullptr; // c becomes nullptr before cf_bf_over and cf_extra!
    agg_con.variant.aggregate.first_constant = &cc_arr;
    assert(llvm_const_from_aggregate(&agg_con, complex_cls_st, &out) == llvm_gen_be_error_t::ok);

    // Test bitfield loop ending when field becomes nullptr (last field is a bitfield) and remaining elements are i32_ty and pad2_ty
    cf_bf1.next = nullptr;
    llvm::StructType* tail_bf_st = llvm::StructType::get(*state.context, {pad2_ty, i8_ty, i32_ty, pad2_ty});
    cc_bf1.next = &elem1;
    elem1.next = &elem2;
    elem2.next = nullptr;
    assert(llvm_const_from_aggregate(&agg_con, tail_bf_st, &out) == llvm_gen_be_error_t::ok);

    // Test error propagation inside field != nullptr non-bitfield evaluate_constant
    cf_arr.next = nullptr;
    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, tail_bf_st, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Test field == nullptr struct loop with excess constants and error propagation
    cls_agg_ty.variant.class_struct_union.field_list = nullptr;
    llvm::Type* one_i32_elem[] = {i32_ty};
    llvm::StructType* one_elem_st = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(one_i32_elem));
    elem1.next = &elem2;
    elem2.next = nullptr;
    agg_con.variant.aggregate.first_constant = &elem1;
    assert(llvm_const_from_aggregate(&agg_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);

    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, one_elem_st, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Phase 1 Tests: ck_complex, ck_imaginary, ck_label_difference, ck_stack_offset, ck_dynamic_init, ck_void
    elem1.next = nullptr;
    elem2.next = nullptr;
    a_type dummy_ptr_ty = {};
    dummy_ptr_ty.kind = tk_pointer;
    dummy_ptr_ty.size = 8;
    a_variable dummy_var = {};
    dummy_var.source_corresp.name = "my_dummy_var";
    dummy_var.type = &dummy_int_ty;

    assert(llvm_const_from_complex(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_complex(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_complex(&dummy_const, f32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant cplx_con = {};
    cplx_con.kind = ck_complex;
    cplx_con.type = &dummy_fp_ty;
    cplx_con.variant.complex_value = nullptr;
    assert(llvm_const_from_complex(&cplx_con, f32_ty, &out) == llvm_gen_be_error_t::invalid_argument);

    an_internal_complex_value cplx_val = {};
    cplx_val.real.bytes[0] = 0; // normal
    cplx_val.imag.bytes[0] = 1; // +inf
    cplx_con.variant.complex_value = &cplx_val;
    llvm::StructType* cplx_st_ty = llvm::StructType::get(*state.context, {f32_ty, f32_ty});
    assert(evaluate_constant(&cplx_con, cplx_st_ty, &out) == llvm_gen_be_error_t::ok);
    // Non-struct expected_ty fallback in llvm_const_from_complex
    assert(llvm_const_from_complex(&cplx_con, f32_ty, &out) == llvm_gen_be_error_t::ok);
    // 1-element struct expected_ty fallback in llvm_const_from_complex
    assert(llvm_const_from_complex(&cplx_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);
    // Invalid float_kind in non-struct expected_ty fallback
    a_type bad_fp_ty = {};
    bad_fp_ty.kind = tk_float;
    bad_fp_ty.variant.float_kind = static_cast<a_float_kind>(99);
    cplx_con.type = &bad_fp_ty;
    assert(llvm_const_from_complex(&cplx_con, f32_ty, &out) == llvm_gen_be_error_t::unsupported_type);
    cplx_con.type = &dummy_fp_ty;

    // ck_imaginary
    assert(llvm_const_from_imaginary(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_imaginary(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_imaginary(&dummy_const, f32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant imag_con = {};
    imag_con.kind = ck_imaginary;
    imag_con.type = &dummy_fp_ty;
    imag_con.variant.float_value.bytes[0] = 0;
    assert(evaluate_constant(&imag_con, f32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&imag_con, cplx_st_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&imag_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);
    imag_con.type = &bad_fp_ty;
    assert(evaluate_constant(&imag_con, one_elem_st, &out) == llvm_gen_be_error_t::unsupported_type);
    imag_con.type = &dummy_fp_ty;

    // ck_label_difference
    assert(llvm_const_from_label_difference(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_label_difference(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_label_difference(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant lbl_diff = {};
    lbl_diff.kind = ck_label_difference;
    lbl_diff.type = &dummy_int_ty;
    lbl_diff.variant.label_difference.to_address = nullptr;
    lbl_diff.variant.label_difference.from_address = nullptr;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);

    a_label lbl_a = {}, lbl_b = {};
    a_constant addr_a = {}, addr_b = {};
    addr_a.kind = ck_address;
    addr_a.type = &dummy_ptr_ty;
    addr_a.variant.address.kind = abk_label;
    addr_a.variant.address.variant.label = &lbl_a;
    addr_b.kind = ck_address;
    addr_b.type = &dummy_ptr_ty;
    addr_b.variant.address.kind = abk_label;
    addr_b.variant.address.variant.label = &lbl_b;

    lbl_diff.variant.label_difference.to_address = &addr_b;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    lbl_diff.variant.label_difference.from_address = &addr_a;
    assert(evaluate_constant(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&lbl_diff, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // Error propagation in label_difference
    lbl_diff.variant.label_difference.to_address = &bad_elem;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
    lbl_diff.variant.label_difference.to_address = &addr_b;
    lbl_diff.variant.label_difference.from_address = &bad_elem;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // ck_stack_offset
    assert(llvm_const_from_stack_offset(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_stack_offset(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_stack_offset(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant stk_con = {};
    stk_con.type = &dummy_int_ty;
    stk_con.variant.integer_value = 16;
    assert(llvm_const_from_stack_offset(&stk_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_stack_offset(&stk_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    stk_con.kind = ck_stack_offset;
    stk_con.variant.stack_offset.offset = 24;
    assert(evaluate_constant(&stk_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
#endif

    // ck_dynamic_init
    assert(llvm_const_from_dynamic_init(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_dynamic_init(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_dynamic_init(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant dyn_con = {};
    dyn_con.kind = ck_dynamic_init;
    dyn_con.type = &dummy_int_ty;
    dyn_con.variant.dynamic_init.ptr = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    a_dynamic_init dip_node = {};
    dip_node.kind = dik_zero;
    dyn_con.variant.dynamic_init.ptr = &dip_node;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    dip_node.kind = dik_constant;
    dip_node.variant.constant.ptr = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dip_node.variant.constant.ptr = &elem1;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    an_expr_node dyn_expr = {};
    dip_node.kind = dik_expression;
    dip_node.variant.expression = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dyn_expr.kind = enk_operation;
    dip_node.variant.expression = &dyn_expr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dyn_expr.kind = enk_constant;
    dyn_expr.variant.constant.ptr = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dyn_expr.variant.constant.ptr = &elem1;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // ck_void
    a_constant void_con = {};
    void_con.kind = ck_void;
    void_con.type = &dummy_int_ty;
    assert(evaluate_constant(&void_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_temporary, abk_typeid, abk_uuidof, abk_param_ref, and subobject_path
    a_constant tmp_addr = {};
    tmp_addr.kind = ck_address;
    tmp_addr.type = &dummy_ptr_ty;
    tmp_addr.variant.address.kind = abk_temporary;
    tmp_addr.variant.address.variant.constant = nullptr;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_constant bad_tmp_target = {};
    bad_tmp_target.kind = ck_integer;
    bad_tmp_target.type = &bad_ty;
    tmp_addr.variant.address.variant.constant = &bad_tmp_target;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    bad_tmp_target.type = &dummy_int_ty;
    bad_tmp_target.kind = static_cast<a_constant_repr_kind>(99);
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    tmp_addr.variant.address.variant.constant = &elem1;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Second lookup hits temporary_constant_cache
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_typeid
    tmp_addr.variant.address.kind = abk_typeid;
    tmp_addr.variant.address.variant.type = &dummy_int_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_uuidof: nullptr type, anonymous class type, named class with uuid_string, enum with uuid_string, cached lookup
    tmp_addr.variant.address.kind = abk_uuidof;
    tmp_addr.variant.address.variant.type = nullptr;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Repeat to hit existing GlobalVariable cache
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_anon_ty = {};
    uuid_anon_ty.kind = tk_pointer;
    uuid_anon_ty.source_corresp.name = "";
    tmp_addr.variant.address.variant.type = &uuid_anon_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_null_name_ty = {};
    uuid_null_name_ty.kind = tk_struct;
    uuid_null_name_ty.source_corresp.name = nullptr;
    state.module->getNamedGlobal("__uuidof_anon")->eraseFromParent();
    tmp_addr.variant.address.variant.type = &uuid_null_name_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_cls_ty = {};
    uuid_cls_ty.kind = tk_class;
    uuid_cls_ty.source_corresp.name = "MyComClass";
    a_class_type_supplement uuid_cls_supp = {};
    uuid_cls_supp.uuid_string = "{01234567-89ab-CDEF-fedc-ba9876543210}";
    uuid_cls_ty.variant.class_struct_union.extra_info = nullptr;
    tmp_addr.variant.address.variant.type = &uuid_cls_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    uuid_cls_ty.source_corresp.name = "MyComClassWithUuid";
    uuid_cls_ty.variant.class_struct_union.extra_info = &uuid_cls_supp;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_enum_ty = {};
    uuid_enum_ty.kind = tk_integer;
    uuid_enum_ty.source_corresp.name = "MyUuidEnum";
    uuid_enum_ty.variant.integer.enum_type = FALSE;
    tmp_addr.variant.address.variant.type = &uuid_enum_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    uuid_enum_ty.source_corresp.name = "MyUuidEnum2";
    uuid_enum_ty.variant.integer.enum_type = TRUE;
    uuid_enum_ty.variant.integer.extra_info = nullptr;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    char enum_uuid_buf[] = "11223344";
    an_integer_type_supplement uuid_enum_supp = {};
    uuid_enum_supp.uuid_string = enum_uuid_buf; // shorter than 32 hex chars to hit *p == '\0'
    uuid_enum_ty.source_corresp.name = "MyUuidEnum3";
    uuid_enum_ty.variant.integer.extra_info = &uuid_enum_supp;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_param_ref: param_num == 0, param_num > size, non-Constant param, Constant param
    tmp_addr.variant.address.kind = abk_param_ref;
    tmp_addr.variant.address.variant.param_ref.param_num = 0;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    tmp_addr.variant.address.variant.param_ref.param_num = 5;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    llvm::Value* non_const_inst = state.builder->CreateAlloca(i32_ty);
    llvm::GlobalVariable* const_param_gv = llvm::cast<llvm::GlobalVariable>(
        state.module->getOrInsertGlobal("const_param_gv", i32_ty));
    state.current_function_params = {non_const_inst, const_param_gv};
    tmp_addr.variant.address.variant.param_ref.param_num = 1;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    tmp_addr.variant.address.variant.param_ref.param_num = 2;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    state.current_function_params.clear();

    // subobject_path on address constant when offset == 0
    a_base_class sub_bc = {};
    sub_bc.offset = 8;
    a_type sub_elem_int_ty = dummy_int_ty;
    sub_elem_int_ty.size = 4;
    a_type sub_zero_int_ty = dummy_int_ty;
    sub_zero_int_ty.size = 0;
    a_type sub_arr_ty = {};
    sub_arr_ty.kind = tk_array;
    sub_arr_ty.variant.array.element_type = &sub_elem_int_ty;
    a_field sub_f_arr = {}, sub_f_scalar = {}, sub_f_nullty = {};
    sub_f_arr.offset = 4;
    sub_f_arr.type = &sub_arr_ty;
    sub_f_scalar.offset = 2;
    sub_f_scalar.type = &sub_zero_int_ty;
    sub_f_nullty.offset = 1;
    sub_f_nullty.type = nullptr;

    a_subobject_path sp1 = {}, sp2 = {}, sp3 = {}, sp4 = {}, sp5 = {}, sp6 = {}, sp6b = {}, sp7 = {};
    sp1.is_base_class = TRUE;
    sp1.variant.base_class = &sub_bc;
    sp1.next = &sp2;

    sp2.is_base_class = TRUE;
    sp2.variant.base_class = nullptr;
    sp2.next = &sp3;

    sp3.is_offset = TRUE; // curr_elem_ty == nullptr -> elem_sz = 1
    sp3.variant.ptr_offset = 3;
    sp3.next = &sp4;

    sp4.variant.field = &sub_f_arr; // sets curr_elem_ty = sub_elem_int_ty (size 4 > 0)
    sp4.next = &sp5;

    sp5.is_offset = TRUE; // curr_elem_ty->size == 4
    sp5.variant.ptr_offset = 2;
    sp5.next = &sp6;

    sp6.variant.field = &sub_f_scalar; // sets curr_elem_ty = sub_zero_int_ty (size 0)
    sp6.next = &sp6b;

    sp6b.is_offset = TRUE; // curr_elem_ty != nullptr && curr_elem_ty->size == 0
    sp6b.variant.ptr_offset = 1;
    sp6b.next = &sp7;

    sp7.variant.field = &sub_f_nullty;
    sp7.next = nullptr;

    a_subobject_path sp_null_field = {};
    sp_null_field.variant.field = nullptr;
    sp7.next = &sp_null_field;

    tmp_addr.variant.address.offset = 0;
    tmp_addr.variant.address.subobject_path = &sp1;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    tmp_addr.variant.address.subobject_path = nullptr;

    // Array designator (single, generic single, generic range, generic reversed range, null subscript, trailing designator) & ck_init_repeat
    a_constant arr_agg = {};
    arr_agg.kind = ck_aggregate;
    arr_agg.type = &sub_arr_ty;

    a_constant d_non_gen = {}, d_val1 = {}, d_gen_single = {}, d_val2 = {},
               d_gen_range = {}, d_val3 = {}, d_gen_rev = {}, d_val4 = {},
               d_gen_null = {}, d_val5 = {}, c_rep = {}, d_trail = {};
    a_constant sub_idx1 = {}, sub_idx2_lo = {}, sub_idx2_hi = {}, sub_rev_lo = {}, sub_rev_hi = {};
    sub_idx1.kind = ck_integer;
    sub_idx1.variant.integer_value = 0;
    sub_idx2_lo.kind = ck_integer;
    sub_idx2_lo.variant.integer_value = 1;
    sub_idx2_hi.kind = ck_integer;
    sub_idx2_hi.variant.integer_value = 2;
    sub_idx2_lo.next = &sub_idx2_hi;
    sub_rev_lo.kind = ck_integer;
    sub_rev_lo.variant.integer_value = 2;
    sub_rev_hi.kind = ck_integer;
    sub_rev_hi.variant.integer_value = 1;
    sub_rev_lo.next = &sub_rev_hi;

    d_non_gen.kind = ck_designator;
    d_non_gen.variant.designator.is_field_designator = FALSE;
    d_non_gen.variant.designator.is_generic = FALSE;
    d_non_gen.variant.designator.variant.array_element = 10; // idx >= expected_elems (4)
    d_non_gen.next = &d_val1;

    d_val1.kind = ck_integer;
    d_val1.type = &dummy_int_ty;
    d_val1.variant.integer_value = 11;
    d_val1.next = &d_gen_single;

    d_gen_single.kind = ck_designator;
    d_gen_single.variant.designator.is_field_designator = FALSE;
    d_gen_single.variant.designator.is_generic = TRUE;
    d_gen_single.variant.designator.variant.subscript = &sub_idx1;
    d_gen_single.next = &d_val2;

    d_val2.kind = ck_integer;
    d_val2.type = &dummy_int_ty;
    d_val2.variant.integer_value = 22;
    d_val2.next = &d_gen_range;

    d_gen_range.kind = ck_designator;
    d_gen_range.variant.designator.is_field_designator = FALSE;
    d_gen_range.variant.designator.is_generic = TRUE;
    d_gen_range.variant.designator.variant.subscript = &sub_idx2_lo;
    d_gen_range.next = &d_val3;

    d_val3.kind = ck_integer;
    d_val3.type = &dummy_int_ty;
    d_val3.variant.integer_value = 33;
    d_val3.next = &d_gen_rev;

    d_gen_rev.kind = ck_designator;
    d_gen_rev.variant.designator.is_field_designator = FALSE;
    d_gen_rev.variant.designator.is_generic = TRUE;
    d_gen_rev.variant.designator.variant.subscript = &sub_rev_lo;
    d_gen_rev.next = &d_val4;

    d_val4.kind = ck_integer;
    d_val4.type = &dummy_int_ty;
    d_val4.variant.integer_value = 44;
    d_val4.next = &d_gen_null;

    d_gen_null.kind = ck_designator;
    d_gen_null.variant.designator.is_field_designator = FALSE;
    d_gen_null.variant.designator.is_generic = TRUE;
    d_gen_null.variant.designator.variant.subscript = nullptr;
    d_gen_null.next = &d_val5;

    d_val5.kind = ck_integer;
    d_val5.type = &dummy_int_ty;
    d_val5.variant.integer_value = 55;
    d_val5.next = &c_rep;

    a_constant extra_arr_elem = {};
    extra_arr_elem.kind = ck_integer;
    extra_arr_elem.type = &dummy_int_ty;
    extra_arr_elem.variant.integer_value = 66;
    extra_arr_elem.next = &d_trail;

    c_rep.kind = ck_init_repeat;
    c_rep.variant.init_repeat.count = 5; // exceeds expected_elems (4) so cur_idx < expected_elems is both true and false!
    c_rep.variant.init_repeat.constant = &elem1;
    elem1.next = nullptr;
    c_rep.next = &extra_arr_elem; // regular element when cur_idx >= expected_elems!

    d_trail.kind = ck_designator;
    d_trail.variant.designator.is_field_designator = FALSE;
    d_trail.next = nullptr; // c->next == nullptr break!

    llvm::ArrayType* arr4_ty = llvm::ArrayType::get(i32_ty, 4);
    arr_agg.variant.aggregate.first_constant = &d_non_gen;
    assert(llvm_const_from_aggregate(&arr_agg, arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // Error propagation in array designator value and ck_init_repeat
    d_non_gen.next = &bad_elem;
    assert(llvm_const_from_aggregate(&arr_agg, arr4_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
    c_rep.variant.init_repeat.constant = &bad_elem;
    arr_agg.variant.aggregate.first_constant = &c_rep;
    assert(llvm_const_from_aggregate(&arr_agg, arr4_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Union with ck_designator and pointer-to-integer coercion + pointer-to-non-integer fallback
    a_constant u_desig = {}, u_ptr_c = {};
    u_desig.kind = ck_designator;
    u_desig.variant.designator.is_field_designator = TRUE;
    u_desig.next = &u_ptr_c;
    u_ptr_c.kind = ck_address;
    u_ptr_c.type = &dummy_ptr_ty;
    u_ptr_c.variant.address.kind = abk_variable;
    u_ptr_c.variant.address.variant.variable = &dummy_var;
    agg_con.type = &un_edg_ty;
    agg_con.variant.aggregate.first_constant = &u_desig;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_aggregate(&agg_con, un_bytes_ty, &out) == llvm_gen_be_error_t::ok);

    // Struct with initializes_empty_object, field designator (direct & generic by name), and designator past end
    a_field sf1 = {}, sf2 = {}, sf3 = {};
    sf1.source_corresp.name = "f1";
    sf1.type = &dummy_int_ty;
    sf1.next = &sf2;
    sf2.source_corresp.name = nullptr;
    sf2.type = &dummy_int_ty;
    sf2.next = &sf3;
    sf3.source_corresp.name = "f3";
    sf3.type = &dummy_int_ty;
    sf3.next = nullptr;

    a_type st_desig_edg_ty = {};
    st_desig_edg_ty.kind = tk_struct;
    st_desig_edg_ty.variant.class_struct_union.field_list = &sf1;
    llvm::StructType* st3_ty = llvm::StructType::get(*state.context, {i32_ty, i32_ty, i32_ty});

    a_constant c_empty = {}, c_fdesig_gen = {}, c_fval = {}, c_fdesig_direct = {},
               c_fdesig_nullname = {}, c_fdesig_nomatch = {};
    c_empty.kind = ck_aggregate;
#if DO_IL_LOWERING
    c_empty.initializes_empty_object = TRUE;
#endif
    c_empty.next = &c_fdesig_gen;

    c_fdesig_gen.kind = ck_designator;
    c_fdesig_gen.variant.designator.is_field_designator = TRUE;
    c_fdesig_gen.variant.designator.is_generic = TRUE;
    c_fdesig_gen.variant.designator.variant.field_name = "f3";
    c_fdesig_gen.next = &c_fval;

    c_fval.kind = ck_integer;
    c_fval.type = &dummy_int_ty;
    c_fval.variant.integer_value = 99;
    c_fval.next = nullptr;

    agg_con.type = &st_desig_edg_ty;
    agg_con.variant.aggregate.first_constant = &c_empty;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);

    // Direct field designator & designator exceeding st_ty->getNumElements()
    c_fdesig_direct.kind = ck_designator;
    c_fdesig_direct.variant.designator.is_field_designator = TRUE;
    c_fdesig_direct.variant.designator.is_generic = FALSE;
    c_fdesig_direct.variant.designator.variant.field = &sf3;
    c_fdesig_direct.next = &c_fval;
    agg_con.variant.aggregate.first_constant = &c_fdesig_direct;
    assert(llvm_const_from_aggregate(&agg_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);

    // Generic field designator with null field_name and non-matching field_name
    c_fdesig_nullname.kind = ck_designator;
    c_fdesig_nullname.variant.designator.is_field_designator = TRUE;
    c_fdesig_nullname.variant.designator.is_generic = TRUE;
    c_fdesig_nullname.variant.designator.variant.field_name = nullptr;
    c_fdesig_nullname.next = &c_fdesig_nomatch;

    c_fdesig_nomatch.kind = ck_designator;
    c_fdesig_nomatch.variant.designator.is_field_designator = TRUE;
    c_fdesig_nomatch.variant.designator.is_generic = TRUE;
    c_fdesig_nomatch.variant.designator.variant.field_name = "nonexistent";
    c_fdesig_nomatch.next = &c_fval;
    c_fval.next = nullptr;
    agg_con.variant.aggregate.first_constant = &c_fdesig_nullname;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);

    // ck_init_repeat inside struct aggregate (valid repeat, count < elements, count exceeding elements, field==nullptr, and error propagation)
    a_constant st_rep = {};
    st_rep.kind = ck_init_repeat;
    st_rep.variant.init_repeat.count = 1;
    st_rep.variant.init_repeat.constant = &c_fval;
    st_rep.next = nullptr;
    agg_con.variant.aggregate.first_constant = &st_rep;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);
    st_rep.variant.init_repeat.count = 5;
    sf2.next = nullptr; // only 2 EDG fields for 3-element st3_ty so field becomes nullptr during repeat!
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);
    st_rep.variant.init_repeat.constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
    sf2.next = &sf3;

    // Test pointer-typed ck_integer constants in llvm_const_from_integer (low==0 && high==0, low==0 && high!=0, low!=0)
    a_constant ptr_int_con = {};
    ptr_int_con.kind = ck_integer;
    ptr_int_con.type = &dummy_int_ty;
    ptr_int_con.variant.integer_value = 0;
    assert(llvm_const_from_integer(&ptr_int_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    ptr_int_con.variant.integer_value = 0x1000;
    assert(llvm_const_from_integer(&ptr_int_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
#if INT128_EXTENSIONS_ALLOWED
    ptr_int_con.variant.integer_value = (static_cast<an_integer_value>(1) << 64);
    assert(llvm_const_from_integer(&ptr_int_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
#endif

    // Phase 2.1: ScalableVectorType constants in llvm_const_from_aggregate (empty, non-empty splat, and error)
    llvm::Type* sv4i32_ty = llvm::ScalableVectorType::get(i32_ty, 4);
    agg_con.variant.aggregate.first_constant = nullptr;
    assert(llvm_const_from_aggregate(&agg_con, sv4i32_ty, &out) == llvm_gen_be_error_t::ok);
    c_fval.next = nullptr;
    agg_con.variant.aggregate.first_constant = &c_fval;
    assert(llvm_const_from_aggregate(&agg_con, sv4i32_ty, &out) == llvm_gen_be_error_t::ok);
    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, sv4i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Section 7.1: ck_stack_offset and ck_template_param in evaluate_constant and llvm_const_from_template_param
    a_constant stack_off_con = {};
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    stack_off_con.kind = ck_stack_offset;
    stack_off_con.variant.stack_offset.offset = 32;
    assert(evaluate_constant(&stack_off_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
#else
    stack_off_con.variant.integer_value = 32;
    assert(llvm_const_from_stack_offset(&stack_off_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
#endif

    assert(llvm_const_from_template_param(nullptr, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_template_param(&c_fval, nullptr, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_template_param(&c_fval, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    a_constant tp_con = {};
    tp_con.kind = ck_template_param;
    tp_con.variant.template_param.kind = tpck_dependent_constant;
    tp_con.variant.template_param.variant.constant = &c_fval;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_address;
    tp_con.variant.template_param.variant.constant = &c_fval;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    an_expr_node tp_expr = {};
    tp_expr.kind = enk_constant;
    tp_expr.variant.constant.ptr = &c_fval;
    tp_con.variant.template_param.kind = tpck_expression;
    tp_con.variant.template_param.variant.expr = &tp_expr;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_sizeof;
    tp_con.variant.template_param.variant.templ_sizeof.type = &dummy_int_ty;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_datasizeof;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_alignof;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_param;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::constant_eval_failure);

#if FIXED_POINT_ALLOWED
    // Workstream 9: Test ck_fixed_point constant evaluation (with integer expected_ty, non-integer expected_ty, null expected_ty, and invalid con->type)
    a_type fx_con_ty = {};
    fx_con_ty.kind = tk_fixed_point;
    fx_con_ty.size = 4;
    a_constant fx_con = {};
    fx_con.kind = ck_fixed_point;
    fx_con.type = &fx_con_ty;
    fx_con.variant.fixed_point_value = 0x4000;
    assert(evaluate_constant(&fx_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&fx_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&fx_con, nullptr, &out) == llvm_gen_be_error_t::ok);
    a_type bad_fx_ty = {};
    bad_fx_ty.kind = static_cast<a_type_kind>(999);
    fx_con.type = &bad_fx_ty;
    assert(evaluate_constant(&fx_con, nullptr, &out) == llvm_gen_be_error_t::unsupported_type);
#endif

    // Phase 7: Test llvm_pack_constant_bitfield_bytes on little-endian, big-endian, and null arguments
    a_field* bf_cur = &bf1;
    a_constant* con_cur = &bf_c1;
    assert(llvm_pack_constant_bitfield_bytes(nullptr, &con_cur, i8_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    a_field* null_bf_cur = nullptr;
    assert(llvm_pack_constant_bitfield_bytes(&null_bf_cur, &con_cur, i8_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, nullptr, i8_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, &con_cur, nullptr, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, &con_cur, i8_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    state.module->setDataLayout("E-m:e-i64:64-n32:64");
    bf_cur = &bf1;
    con_cur = &bf_c1;
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, &con_cur, i8_ty, &out) == llvm_gen_be_error_t::ok);
    state.module->setDataLayout("");
  }
  
  printf("All passed\n");
  return 0;
}
namespace edg {
  /**
   * @brief Test stub or helper for `free_general`.
   * @details Provides mock or helper behavior for `free_general` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `free_general`.
   * @param[in] arg_1 Parameter `arg_1` for `free_general`.
   */
  void free_general(void* arg_0, unsigned long arg_1) {}
  /**
   * @brief Test stub or helper for `alloc_general`.
   * @details Provides mock or helper behavior for `alloc_general` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `alloc_general`.
   * @return Result of `alloc_general` (char*).
   */
  char* alloc_general(unsigned long arg_0) { return nullptr; }
  /**
   * @brief Test stub or helper for `assertion_failed`.
   * @details Provides mock or helper behavior for `assertion_failed` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `assertion_failed`.
   * @param[in] arg_1 Parameter `arg_1` for `assertion_failed`.
   * @param[in] arg_2 Parameter `arg_2` for `assertion_failed`.
   * @param[in] arg_3 Parameter `arg_3` for `assertion_failed`.
   * @param[in] arg_4 Parameter `arg_4` for `assertion_failed`.
   */
  void assertion_failed(char const* arg_0, int arg_1, char const* arg_2, char const* arg_3, char const* arg_4) { abort(); }
  /**
   * @brief Test stub or helper for `insufficient_address_space`.
   * @details Provides mock or helper behavior for `insufficient_address_space` within `tests/test_llvm_const_evaluation.cpp`.
   */
  void insufficient_address_space() { abort(); }
}
