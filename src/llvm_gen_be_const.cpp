/**
 * @file llvm_gen_be_const.cpp
 * @brief Constant evaluator subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end constant structures (a_constant_ptr) into LLVM IR
 * constants (llvm::Constant*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_const.h"

#include <cstring>
#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "fe_common.h"

#include "float_pt.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_ptrmem.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Translates an EDG integer constant (`ck_integer`) into an `llvm::ConstantInt`.
 * @details Supports signed and unsigned integral values up to 128 bits (`APInt`).
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM integer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_integer(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  bool is_unsigned = (con->type->kind == tk_integer)
                         ? !int_kind_is_signed[con->type->variant.integer.int_kind]
                         : false;
  uint64_t low = static_cast<uint64_t>(con->variant.integer_value);
  uint64_t high = 0;
#if INT128_EXTENSIONS_ALLOWED
  if (sizeof(an_integer_value) > 8) {
    high = static_cast<uint64_t>(con->variant.integer_value >> 64);
  }
#endif

  if (expected_ty->isPointerTy()) {
    if (low == 0) {
      *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expected_ty));
    } else {
      llvm::Constant* int_c = llvm::ConstantInt::get(
          llvm::Type::getInt64Ty(*be_state->context), low);
      *out_const = llvm::ConstantExpr::getIntToPtr(int_c, expected_ty);
    }
    return llvm_gen_be_error_t::ok;
  }

  unsigned bit_width = expected_ty->getIntegerBitWidth();
  llvm::APInt api(128, {low, high});
  if (bit_width != 128) {
    if (is_unsigned) {
      api = api.zextOrTrunc(bit_width);
    } else {
      api = api.sextOrTrunc(bit_width);
    }
  }

  *out_const = llvm::ConstantInt::get(expected_ty->getContext(), api);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Helper that converts an `an_internal_float_value` into an `llvm::ConstantFP`.
 * @details Handles normal values, positive/negative infinity, and quiet NaNs.
 * @param[in] fk The EDG float kind (`a_float_kind`).
 * @param[in] fval Pointer to the internal floating-point value.
 * @param[in] fp_ty The target LLVM floating-point type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t build_fp_constant_from_internal_value(
    a_float_kind fk,
    an_internal_float_value* fval,
    llvm::Type* fp_ty,
    llvm::Constant** out_const) noexcept {
  a_boolean pos_inf = FALSE, neg_inf = FALSE, nan = FALSE;
  a_number_buffer hex_str = fp_to_string(fk, fval, &pos_inf, &neg_inf, &nan);

  if (pos_inf) {
    *out_const = llvm::ConstantFP::getInfinity(fp_ty, false);
  } else if (neg_inf) {
    *out_const = llvm::ConstantFP::getInfinity(fp_ty, true);
  } else if (nan) {
    *out_const = llvm::ConstantFP::getQNaN(fp_ty);
  } else {
    llvm::StringRef str_ref(hex_str.as_temp_characters());
    *out_const = llvm::ConstantFP::get(fp_ty, str_ref);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG floating-point constant (`ck_float`) into an `llvm::ConstantFP`.
 * @details Handles normal values, positive/negative infinity, and quiet NaNs.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM floating-point type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_float(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_float_kind fk = con->type->variant.float_kind;
  return build_fp_constant_from_internal_value(
      fk, &con->variant.float_value, expected_ty, out_const);
}

/**
 * @brief Translates an EDG complex constant (`ck_complex`) into a two-element `llvm::ConstantStruct`.
 * @details Extracts both real and imaginary components (`an_internal_complex_value`) using `fp_to_string`,
 *          handling normal values, positive/negative infinity, and quiet NaNs independently.
 * @param[in] con Pointer to the EDG complex constant structure.
 * @param[in] expected_ty The target LLVM struct type `{ elem_ty, elem_ty }`.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_complex(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const || !con->variant.complex_value) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::StructType* st_ty = nullptr;
  llvm::Type* elem_ty = nullptr;
  if (expected_ty->isStructTy() && expected_ty->getStructNumElements() == 2) {
    st_ty = llvm::cast<llvm::StructType>(expected_ty);
    elem_ty = st_ty->getElementType(0);
  } else {
    LLVM_GEN_BE_CHECK(llvm_type_from_float(con->type, &elem_ty));
    st_ty = llvm::StructType::get(*be_state->context, {elem_ty, elem_ty});
  }

  a_float_kind fk = con->type->variant.float_kind;
  llvm::Constant* real_c = nullptr;
  llvm::Constant* imag_c = nullptr;
  llvm_gen_be_error_t err = build_fp_constant_from_internal_value(
      fk, &con->variant.complex_value->real, elem_ty, &real_c);
  err = build_fp_constant_from_internal_value(
      fk, &con->variant.complex_value->imag, elem_ty, &imag_c);

  *out_const = llvm::ConstantStruct::get(st_ty, {real_c, imag_c});
  return err;
}

/**
 * @brief Translates an EDG C99 imaginary constant (`ck_imaginary`) into an `llvm::ConstantFP` or `{0.0, imag}` struct.
 * @details Evaluates `con->variant.float_value` and produces either a scalar floating-point constant
 *          or a two-element `{ 0.0, imag }` `llvm::ConstantStruct` when `expected_ty` is a struct.
 * @param[in] con Pointer to the EDG imaginary constant structure.
 * @param[in] expected_ty The target LLVM floating-point or complex struct type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_imaginary(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_float_kind fk = con->type->variant.float_kind;
  if (expected_ty->isStructTy() && expected_ty->getStructNumElements() == 2) {
    auto* st_ty = llvm::cast<llvm::StructType>(expected_ty);
    llvm::Type* elem_ty = st_ty->getElementType(0);
    llvm::Constant* zero_real = llvm::ConstantFP::getZero(elem_ty);
    llvm::Constant* imag_c = nullptr;
    llvm_gen_be_error_t err = build_fp_constant_from_internal_value(
        fk, &con->variant.float_value, elem_ty, &imag_c);
    *out_const = llvm::ConstantStruct::get(st_ty, {zero_real, imag_c});
    return err;
  }

  llvm::Type* fp_ty = expected_ty;
  if (!fp_ty->isFloatingPointTy()) {
    LLVM_GEN_BE_CHECK(llvm_type_from_float(con->type, &fp_ty));
  }
  return build_fp_constant_from_internal_value(
      fk, &con->variant.float_value, fp_ty, out_const);
}

/**
 * @brief Translates a GNU label-difference constant (`ck_label_difference`, `&&to - &&from`) into an `llvm::Constant*`.
 * @details Evaluates `to_address` and `from_address` as `llvm::BlockAddress*`, converts both via `getPtrToInt`,
 *          and emits `llvm::ConstantExpr::getSub(to_int, from_int)`.
 * @param[in] con Pointer to the EDG label-difference constant structure.
 * @param[in] expected_ty The target LLVM integer type for the offset.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_label_difference(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const ||
      !con->variant.label_difference.to_address ||
      !con->variant.label_difference.from_address) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Constant* to_addr = nullptr;
  llvm::Constant* from_addr = nullptr;
  LLVM_GEN_BE_CHECK(evaluate_constant(
      con->variant.label_difference.to_address, ptr_ty, &to_addr));
  LLVM_GEN_BE_CHECK(evaluate_constant(
      con->variant.label_difference.from_address, ptr_ty, &from_addr));

  llvm::Type* int_ty = expected_ty->isIntegerTy()
                           ? expected_ty
                           : be_state->module->getDataLayout().getIntPtrType(*be_state->context);
  llvm::Constant* to_int = llvm::ConstantExpr::getPtrToInt(to_addr, int_ty);
  llvm::Constant* from_int = llvm::ConstantExpr::getPtrToInt(from_addr, int_ty);
  *out_const = llvm::ConstantExpr::getSub(to_int, from_int);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates a stack-offset constant (`ck_stack_offset`) into an `llvm::Constant*`.
 * @details Evaluates the stack offset constant relative to a local variable for EH table lowering.
 * @param[in] con Pointer to the EDG stack-offset constant structure.
 * @param[in] expected_ty The target LLVM integer or pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_stack_offset(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  uint64_t off_val = 0;
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
  off_val = static_cast<uint64_t>(con->variant.stack_offset.offset);
#else
  off_val = static_cast<uint64_t>(con->variant.integer_value);
#endif
  if (expected_ty->isIntegerTy()) {
    *out_const = llvm::ConstantInt::get(expected_ty, off_val);
  } else {
    *out_const = llvm::Constant::getNullValue(expected_ty);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an embedded dynamic initialization constant (`ck_dynamic_init`) into an `llvm::Constant*`.
 * @details Folds `dik_constant` or constant `dik_expression` entries to `llvm::Constant*`, and produces
 *          a zero-initialized constant placeholder for runtime-initialized subobjects.
 * @param[in] con Pointer to the EDG dynamic-init constant structure.
 * @param[in] expected_ty The target LLVM type of the subobject.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_dynamic_init(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_dynamic_init_ptr dip = con->variant.dynamic_init.ptr;
  if (dip != nullptr) {
    if (dip->kind == dik_constant && dip->variant.constant.ptr != nullptr) {
      return evaluate_constant(dip->variant.constant.ptr, expected_ty, out_const);
    }
    if (dip->kind == dik_expression && dip->variant.expression != nullptr &&
        dip->variant.expression->kind == enk_constant &&
        dip->variant.expression->variant.constant.ptr != nullptr) {
      return evaluate_constant(
          dip->variant.expression->variant.constant.ptr, expected_ty, out_const);
    }
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates a C++ template-parameter constant (`ck_template_param`) into an `llvm::Constant*`.
 * @details Resolves folded `tpck_dependent_constant`, `tpck_address`, `tpck_expression`, `tpck_sizeof`,
 *          `tpck_datasizeof`, and `tpck_alignof` nodes, or returns `constant_eval_failure` for
 *          uninstantiated dependent template parameters.
 * @param[in] con Pointer to the EDG template-parameter constant structure.
 * @param[in] expected_ty The target LLVM type of the constant.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_template_param(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_template_param_constant_kind tp_kind = con->variant.template_param.kind;
  if (tp_kind == tpck_dependent_constant || tp_kind == tpck_address) {
    return evaluate_constant(
        con->variant.template_param.variant.constant, expected_ty, out_const);
  }
  if (tp_kind == tpck_expression) {
    return evaluate_constant(
        con->variant.template_param.variant.expr->variant.constant.ptr, expected_ty, out_const);
  }
  if (tp_kind == tpck_sizeof || tp_kind == tpck_datasizeof || tp_kind == tpck_alignof) {
    a_type_ptr sz_ty = skip_typerefs(con->variant.template_param.variant.templ_sizeof.type);
    uint64_t val = (tp_kind == tpck_alignof)
                       ? static_cast<uint64_t>(f_alignment_of_type(sz_ty))
                       : static_cast<uint64_t>(sz_ty->size);
    *out_const = llvm::ConstantInt::get(expected_ty, val);
    return llvm_gen_be_error_t::ok;
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::constant_eval_failure;
}

/**
 * @brief Translates an EDG string constant (`ck_string`) into an `llvm::ConstantDataArray` or global string pointer.
 * @details Supports narrow (`i8`), UTF-16 (`i16`), and UTF-32/wide (`i32`) character arrays with deduplication.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM array or pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_string(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_targ_size_t len = con->variant.string.length;
  const char* chars = con->variant.string.value;

  llvm::Type* actual_array_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(con->type, &actual_array_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* elem_ty = nullptr;
  if (actual_array_ty->isArrayTy()) {
    elem_ty = actual_array_ty->getArrayElementType();
  } else {
    elem_ty = llvm::Type::getInt8Ty(*be_state->context);
  }

  llvm::Constant* arr = nullptr;
  if (elem_ty->isIntegerTy(16)) {
    llvm::ArrayRef<uint16_t> wide_chars(reinterpret_cast<const uint16_t*>(chars), len / 2);
    arr = llvm::ConstantDataArray::get(*be_state->context, wide_chars);
  } else if (elem_ty->isIntegerTy(32)) {
    llvm::ArrayRef<uint32_t> wide_chars(reinterpret_cast<const uint32_t*>(chars), len / 4);
    arr = llvm::ConstantDataArray::get(*be_state->context, wide_chars);
  } else {
    arr = llvm::ConstantDataArray::get(
        *be_state->context,
        llvm::ArrayRef<uint8_t>(reinterpret_cast<const uint8_t*>(chars), len));
  }

  if (expected_ty->isPointerTy()) {
    std::string str_key(chars, len);
    if (be_state->string_literal_cache.count(str_key)) {
      *out_const = be_state->string_literal_cache[str_key];
      return llvm_gen_be_error_t::ok;
    }
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(
        *be_state->module, arr->getType(), true,
        llvm::GlobalValue::PrivateLinkage, arr, ".str");
    be_state->string_literal_cache[str_key] = gv;
    *out_const = gv;
  } else {
    *out_const = arr;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Converts a single hexadecimal character into its 4-bit nibble value.
 * @details Supports `'0'`-`'9'`, `'a'`-`'f'`, and `'A'`-`'F'`.
 * @param[in] ch The ASCII character to parse.
 * @param[out] out_nibble Receives the 4-bit integer value (`0`-`15`), or `0` if non-hex.
 * @param[out] out_valid Receives `true` if @p ch is a valid hexadecimal digit.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t parse_hex_nibble(
    char ch,
    uint8_t* out_nibble,
    bool* out_valid) noexcept {
  if (ch >= '0' && ch <= '9') {
    *out_nibble = static_cast<uint8_t>(ch - '0');
    *out_valid = true;
  } else if (ch >= 'a' && ch <= 'f') {
    *out_nibble = static_cast<uint8_t>(10 + (ch - 'a'));
    *out_valid = true;
  } else if (ch >= 'A' && ch <= 'F') {
    *out_nibble = static_cast<uint8_t>(10 + (ch - 'A'));
    *out_valid = true;
  } else {
    *out_nibble = 0;
    *out_valid = false;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits or retrieves the Microsoft `_GUID` constant global for an `abk_uuidof` type.
 * @details Parses the 32 hex digits of `uuid_string` into `{ i32, i16, i16, [8 x i8] }`
 *          and creates a LinkOnceODR global `__uuidof_<name>`.
 * @param[in] uuid_type Pointer to the EDG type associated with `__uuidof` (or nullptr for zero GUID).
 * @param[out] out_guid_gv Receives the `llvm::Constant*` pointing to the `_GUID` global.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_or_create_uuidof_global(
    a_type_ptr uuid_type,
    llvm::Constant** out_guid_gv) noexcept {
  std::string sym_name = "__uuidof_zero";
  const char* uuid_str = nullptr;
  if (uuid_type != nullptr) {
    a_type_ptr ut = skip_typerefs(uuid_type);
    if (ut->source_corresp.name != nullptr && ut->source_corresp.name[0] != '\0') {
      sym_name = std::string("__uuidof_") + ut->source_corresp.name;
    } else {
      sym_name = "__uuidof_anon";
    }
    if (ut->kind == tk_class || ut->kind == tk_struct) {
      if (ut->variant.class_struct_union.extra_info != nullptr) {
        uuid_str = ut->variant.class_struct_union.extra_info->uuid_string;
      }
    } else if (ut->kind == tk_integer && ut->variant.integer.enum_type &&
               ut->variant.integer.extra_info != nullptr) {
      uuid_str = ut->variant.integer.extra_info->uuid_string;
    }
  }

  if (llvm::GlobalVariable* existing = be_state->module->getNamedGlobal(sym_name)) {
    *out_guid_gv = existing;
    return llvm_gen_be_error_t::ok;
  }

  uint8_t nibbles[32] = {0};
  if (uuid_str != nullptr) {
    size_t idx = 0;
    for (const char* p = uuid_str; *p != '\0' && idx < 32; ++p) {
      uint8_t nib = 0;
      bool valid = false;
      LLVM_GEN_BE_ASSERT_OK(parse_hex_nibble(*p, &nib, &valid));
      if (valid) {
        nibbles[idx++] = nib;
      }
    }
  }

  uint32_t data1 = 0;
  for (int i = 0; i < 8; ++i) {
    data1 = (data1 << 4) | nibbles[i];
  }
  uint16_t data2 = static_cast<uint16_t>(
      (nibbles[8] << 12) | (nibbles[9] << 8) | (nibbles[10] << 4) | nibbles[11]);
  uint16_t data3 = static_cast<uint16_t>(
      (nibbles[12] << 12) | (nibbles[13] << 8) | (nibbles[14] << 4) | nibbles[15]);
  uint8_t data4[8] = {0};
  for (int i = 0; i < 8; ++i) {
    data4[i] = static_cast<uint8_t>((nibbles[16 + i * 2] << 4) | nibbles[17 + i * 2]);
  }

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* i16_ty = llvm::Type::getInt16Ty(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::ArrayType* arr8_ty = llvm::ArrayType::get(i8_ty, 8);
  llvm::StructType* guid_ty = llvm::StructType::get(
      *be_state->context, {i32_ty, i16_ty, i16_ty, arr8_ty});

  llvm::Constant* guid_init = llvm::ConstantStruct::get(
      guid_ty,
      {llvm::ConstantInt::get(i32_ty, data1),
       llvm::ConstantInt::get(i16_ty, data2),
       llvm::ConstantInt::get(i16_ty, data3),
       llvm::ConstantDataArray::get(*be_state->context, llvm::ArrayRef<uint8_t>(data4, 8))});

  llvm::GlobalVariable* gv = new llvm::GlobalVariable(
      *be_state->module, guid_ty, true,
      llvm::GlobalValue::LinkOnceODRLinkage, guid_init, sym_name);
  *out_guid_gv = gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes additional byte offset from an EDG `a_subobject_path_ptr` chain when `offset == 0`.
 * @details Traverses base class, field, and array element subobject path entries.
 * @param[in] path Pointer to the head of the `a_subobject_path` list.
 * @param[out] out_extra_offset Receives the accumulated byte offset.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t compute_subobject_path_offset(
    a_subobject_path_ptr path,
    int64_t* out_extra_offset) noexcept {
  int64_t total = 0;
  a_type_ptr curr_elem_ty = nullptr;
  for (a_subobject_path_ptr p = path; p != nullptr; p = p->next) {
    if (p->is_base_class) {
      if (p->variant.base_class != nullptr) {
        total += static_cast<int64_t>(p->variant.base_class->offset);
      }
    } else if (p->is_offset) {
      int64_t elem_sz = (curr_elem_ty != nullptr && curr_elem_ty->size > 0)
                            ? static_cast<int64_t>(curr_elem_ty->size)
                            : 1;
      total += static_cast<int64_t>(p->variant.ptr_offset) * elem_sz;
    } else if (p->variant.field != nullptr) {
      total += static_cast<int64_t>(p->variant.field->offset);
      if (p->variant.field->type != nullptr) {
        a_type_ptr ft = skip_typerefs(p->variant.field->type);
        if (ft->kind == tk_array) {
          curr_elem_ty = ft->variant.array.element_type;
        } else {
          curr_elem_ty = ft;
        }
      }
    }
  }
  *out_extra_offset = total;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG address constant (`ck_address`) into an LLVM global, function, or `BlockAddress` constant.
 * @details Supports `abk_label`, `abk_variable`, `abk_routine`, `abk_constant`, `abk_temporary`,
 *          `abk_typeid`, `abk_uuidof`, and `abk_param_ref`, applying byte offsets and `subobject_path`.
 * @param[in] con Pointer to the EDG address constant structure.
 * @param[in] expected_ty The target LLVM pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_address(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Constant* base = nullptr;

  if (con->variant.address.kind == abk_label) {
    a_label_ptr label = con->variant.address.variant.label;
    llvm::BasicBlock* label_bb = nullptr;
    if (be_state->label_blocks.count(label)) {
      label_bb = be_state->label_blocks[label];
    } else {
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
      label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
      be_state->label_blocks[label] = label_bb;
    }
    if (!label_bb->getParent()) {
      label_bb->insertInto(be_state->builder->GetInsertBlock()->getParent());
    }
    base = llvm::BlockAddress::get(label_bb->getParent(), label_bb);
  } else if (con->variant.address.kind == abk_variable) {
    a_variable_ptr var = con->variant.address.variant.variable;
    if (!var || !var->source_corresp.name) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    const char* sym_name = nullptr;
    LLVM_GEN_BE_ASSERT_OK(llvm_get_variable_symbol_name(var, &sym_name));
    llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(sym_name);
    if (!gvar) {
      llvm::Type* var_ty = nullptr;
      llvm_gen_be_error_t err = get_llvm_type(var->type, &var_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      gvar = llvm::cast<llvm::GlobalVariable>(
          be_state->module->getOrInsertGlobal(sym_name, var_ty));
    }
    base = gvar;
  } else if (con->variant.address.kind == abk_routine) {
    a_routine_ptr routine = con->variant.address.variant.routine;
    if (!routine || !routine->source_corresp.name) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) {
      llvm::FunctionType* func_ty = nullptr;
      llvm_gen_be_error_t err = llvm_type_from_routine(routine->type, &func_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      func = llvm::Function::Create(
          func_ty, llvm::GlobalValue::ExternalLinkage,
          routine->source_corresp.name, be_state->module.get());
    }
    base = func;
  } else if (con->variant.address.kind == abk_constant) {
    a_constant_ptr target = con->variant.address.variant.constant;
    llvm::Type* target_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(target->type, &target_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* target_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    err = evaluate_constant(target, target_ptr_ty, &base);
    if (err != llvm_gen_be_error_t::ok) return err;
  } else if (con->variant.address.kind == abk_temporary) {
    a_constant_ptr target = con->variant.address.variant.constant;
    if (!target) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    auto it = be_state->temporary_constant_cache.find(target);
    if (it != be_state->temporary_constant_cache.end()) {
      base = it->second;
    } else {
      llvm::Type* target_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_llvm_type(target->type, &target_ty));
      llvm::Constant* init_val = nullptr;
      LLVM_GEN_BE_CHECK(evaluate_constant(target, target_ty, &init_val));
      llvm::GlobalVariable* tmp_gv = new llvm::GlobalVariable(
          *be_state->module, init_val->getType(), true,
          llvm::GlobalValue::PrivateLinkage, init_val, ".ref.tmp");
      be_state->temporary_constant_cache[target] = tmp_gv;
      base = tmp_gv;
    }
  } else if (con->variant.address.kind == abk_typeid) {
    LLVM_GEN_BE_ASSERT_OK(get_typeinfo_global(con->variant.address.variant.type, &base));
  } else if (con->variant.address.kind == abk_uuidof) {
    LLVM_GEN_BE_ASSERT_OK(get_or_create_uuidof_global(con->variant.address.variant.type, &base));
  } else if (con->variant.address.kind == abk_param_ref) {
    unsigned int pnum = con->variant.address.variant.param_ref.param_num;
    if (pnum > 0 && pnum <= be_state->current_function_params.size()) {
      llvm::Value* pval = be_state->current_function_params[pnum - 1];
      if (auto* cval = llvm::dyn_cast_or_null<llvm::Constant>(pval)) {
        base = cval;
      }
    }
    if (!base) {
      std::string pname = "__param_ref_" + std::to_string(pnum);
      llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
      base = llvm::cast<llvm::GlobalVariable>(
          be_state->module->getOrInsertGlobal(pname, i8_ty));
    }
  } else {
    *out_const = llvm::Constant::getNullValue(expected_ty);
    return llvm_gen_be_error_t::ok;
  }

  int64_t total_offset = static_cast<int64_t>(con->variant.address.offset);
  if (total_offset == 0 && con->variant.address.subobject_path != nullptr) {
    LLVM_GEN_BE_ASSERT_OK(compute_subobject_path_offset(
        con->variant.address.subobject_path, &total_offset));
  }

  if (total_offset != 0) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Constant* offset_val = llvm::ConstantInt::get(
        be_state->module->getDataLayout().getIntPtrType(*be_state->context),
        static_cast<uint64_t>(total_offset));
    base = llvm::ConstantExpr::getGetElementPtr(i8_ty, base, offset_val);
  }

  *out_const = base;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves the target array index range `[start_idx, end_idx]` for a `ck_designator` entry.
 * @details Handles both non-generic `array_element` and generic `subscript` (single or `[first ... last]` range).
 * @param[in] desig Pointer to the `ck_designator` constant entry.
 * @param[out] out_start Receives the starting array index.
 * @param[out] out_end Receives the inclusive ending array index.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t resolve_array_designator_range(
    a_constant_ptr desig,
    uint64_t* out_start,
    uint64_t* out_end) noexcept {
  if (desig->variant.designator.is_generic) {
    a_constant_ptr sub = desig->variant.designator.variant.subscript;
    if (sub != nullptr) {
      uint64_t first = static_cast<uint64_t>(sub->variant.integer_value);
      uint64_t last = (sub->next != nullptr)
                          ? static_cast<uint64_t>(sub->next->variant.integer_value)
                          : first;
      *out_start = first;
      *out_end = (last >= first) ? last : first;
      return llvm_gen_be_error_t::ok;
    }
  }
  uint64_t idx = static_cast<uint64_t>(desig->variant.designator.variant.array_element);
  *out_start = idx;
  *out_end = idx;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Packs a consecutive run of constant bitfield initializers into a target-endian integer constant.
 * @details Handles both little-endian (`LSB`-first) and big-endian (`MSB`-first) bitfield placement
 *          within @p unit_ty and advances `*io_field` and `*io_con` past the packed bitfields.
 * @param[in,out] io_field Pointer to the current `a_field_ptr` bitfield cursor.
 * @param[in,out] io_con Pointer to the current `a_constant_ptr` initializer cursor.
 * @param[in] unit_ty Target LLVM integer type representing the packed bitfield storage unit.
 * @param[out] out_packed_const Receives the packed `llvm::Constant*` integer for the storage unit.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t llvm_pack_constant_bitfield_bytes(
    a_field_ptr* io_field,
    a_constant_ptr* io_con,
    llvm::Type* unit_ty,
    llvm::Constant** out_packed_const) noexcept {
  if (!io_field || !*io_field || !io_con || !unit_ty || !out_packed_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  a_field_ptr field = *io_field;
  a_constant_ptr c = *io_con;
  unsigned unit_bits = unit_ty->getIntegerBitWidth();
  uint64_t unit_start_offset = field->offset;
  llvm::APInt unit_val(unit_bits, 0);
  bool is_big_endian = be_state->module->getDataLayout().isBigEndian();

  while (field && field->is_bit_field) {
    uint64_t bit_off = (field->offset - unit_start_offset) * 8 + field->offset_bit_remainder;
    uint64_t bit_sz = field->bit_size;
    if (bit_off + bit_sz > unit_bits) {
      break;
    }
    if (c && c->kind == ck_integer && bit_sz > 0) {
      llvm::Constant* bf_const = nullptr;
      LLVM_GEN_BE_ASSERT_OK(llvm_const_from_integer(c, unit_ty, &bf_const));
      auto* ci = llvm::cast<llvm::ConstantInt>(bf_const);
      llvm::APInt mask = llvm::APInt::getLowBitsSet(unit_bits, static_cast<unsigned>(bit_sz));
      unsigned shift_amt = is_big_endian
                               ? static_cast<unsigned>(unit_bits - (bit_off + bit_sz))
                               : static_cast<unsigned>(bit_off);
      llvm::APInt shifted = (ci->getValue().zextOrTrunc(unit_bits) & mask).shl(shift_amt);
      unit_val |= shifted;
    }
    field = field->next;
    if (c) c = c->next;
  }

  *io_field = field;
  *io_con = c;
  *out_packed_const = llvm::ConstantInt::get(unit_ty, unit_val);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG aggregate constant (`ck_aggregate`) into an `llvm::ConstantArray` or `llvm::ConstantStruct`.
 * @details Handles arrays (including `ck_designator` and `ck_init_repeat`), structs (including sparse/out-of-order
 *          field designators, empty base optimization entries, and bitfields), and unions.
 * @param[in] con Pointer to the EDG aggregate constant structure.
 * @param[in] expected_ty The target LLVM array or struct type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_aggregate(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (expected_ty->isVectorTy()) {
    if (auto* sv_ty = llvm::dyn_cast<llvm::ScalableVectorType>(expected_ty)) {
      llvm::Type* elem_ty = sv_ty->getElementType();
      a_constant_ptr c = con->variant.aggregate.first_constant;
      if (c != nullptr) {
        llvm::Constant* elem_const = nullptr;
        llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
        if (err != llvm_gen_be_error_t::ok) return err;
        *out_const = llvm::ConstantVector::getSplat(sv_ty->getElementCount(), elem_const);
      } else {
        *out_const = llvm::Constant::getNullValue(expected_ty);
      }
      return llvm_gen_be_error_t::ok;
    }
    auto* vec_ty = llvm::cast<llvm::FixedVectorType>(expected_ty);
    llvm::Type* elem_ty = vec_ty->getElementType();
    std::vector<llvm::Constant*> elems;
    for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
      llvm::Constant* elem_const = nullptr;
      llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
      if (err != llvm_gen_be_error_t::ok) return err;
      elems.push_back(elem_const);
    }
    unsigned expected_elems = vec_ty->getNumElements();
    while (elems.size() < expected_elems) {
      elems.push_back(llvm::Constant::getNullValue(elem_ty));
    }
    *out_const = llvm::ConstantVector::get(elems);
    return llvm_gen_be_error_t::ok;
  }

  if (expected_ty->isArrayTy()) {
    llvm::Type* elem_ty = expected_ty->getArrayElementType();
    unsigned expected_elems = static_cast<unsigned>(expected_ty->getArrayNumElements());
    std::vector<llvm::Constant*> elems(expected_elems, llvm::Constant::getNullValue(elem_ty));
    unsigned cur_idx = 0;

    for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
      if (c->kind == ck_designator) {
        uint64_t start_idx = 0;
        uint64_t end_idx = 0;
        LLVM_GEN_BE_ASSERT_OK(resolve_array_designator_range(c, &start_idx, &end_idx));
        c = c->next;
        if (!c) break;
        llvm::Constant* elem_const = nullptr;
        LLVM_GEN_BE_CHECK(evaluate_constant(c, elem_ty, &elem_const));
        for (uint64_t idx = start_idx; idx <= end_idx; ++idx) {
          if (idx < expected_elems) {
            elems[idx] = elem_const;
          }
        }
        cur_idx = static_cast<unsigned>(end_idx + 1);
        continue;
      }
      if (c->kind == ck_init_repeat) {
        uint64_t count = c->variant.init_repeat.count;
        llvm::Constant* rep_const = nullptr;
        LLVM_GEN_BE_CHECK(evaluate_constant(c->variant.init_repeat.constant, elem_ty, &rep_const));
        for (uint64_t r = 0; r < count; ++r) {
          if (cur_idx < expected_elems) {
            elems[cur_idx++] = rep_const;
          }
        }
        continue;
      }
      llvm::Constant* elem_const = nullptr;
      llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (cur_idx < expected_elems) {
        elems[cur_idx++] = elem_const;
      }
    }
    *out_const = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(expected_ty), elems);
    return llvm_gen_be_error_t::ok;
  } else if (expected_ty->isStructTy()) {
    llvm::StructType* st_ty = llvm::cast<llvm::StructType>(expected_ty);
    if (con->type->kind == tk_union) {
      llvm::Constant* active_val = nullptr;
      a_constant_ptr c = con->variant.aggregate.first_constant;
      if (c && c->kind == ck_designator) {
        c = c->next;
      }
      if (c && st_ty->getNumElements() > 0) {
        llvm::Type* target_ty = nullptr;
        llvm_gen_be_error_t err = get_llvm_type(c->type, &target_ty);
        if (err == llvm_gen_be_error_t::ok) {
          err = evaluate_constant(c, target_ty, &active_val);
          if (err == llvm_gen_be_error_t::ok && target_ty != st_ty->getElementType(0)) {
            llvm::Type* elem0_ty = st_ty->getElementType(0);
            if (active_val->getType()->isIntegerTy() && elem0_ty->isIntegerTy()) {
              llvm::APInt val_ap = llvm::cast<llvm::ConstantInt>(active_val)->getValue();
              active_val = llvm::ConstantInt::get(
                  elem0_ty->getContext(),
                  val_ap.zextOrTrunc(elem0_ty->getIntegerBitWidth()));
            } else if (elem0_ty->isArrayTy() && elem0_ty->getArrayElementType()->isIntegerTy(8) &&
                       active_val->getType()->isIntegerTy()) {
              uint64_t num_bytes = elem0_ty->getArrayNumElements();
              llvm::APInt val_ap = llvm::cast<llvm::ConstantInt>(active_val)->getValue();
              std::vector<uint8_t> bytes(num_bytes, 0);
              uint64_t val_bytes = (val_ap.getBitWidth() + 7) / 8;
              for (uint64_t b = 0; b < num_bytes && b < val_bytes; ++b) {
                bytes[b] = static_cast<uint8_t>(val_ap.extractBitsAsZExtValue(8, b * 8));
              }
              active_val = llvm::ConstantDataArray::get(*be_state->context, bytes);
            } else if (active_val->getType()->isPointerTy() && elem0_ty->isIntegerTy()) {
              active_val = llvm::ConstantExpr::getPtrToInt(active_val, elem0_ty);
            } else {
              active_val = nullptr;
            }
          }
        }
      }
      std::vector<llvm::Constant*> elems;
      if (active_val) {
        elems.push_back(active_val);
      } else if (st_ty->getNumElements() > 0) {
        elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(0)));
      }
      for (unsigned i = 1; i < st_ty->getNumElements(); i++) {
        elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
      }
      *out_const = llvm::ConstantStruct::get(st_ty, elems);
      return llvm_gen_be_error_t::ok;
    }

    a_field_ptr field_head = (con->type->kind == tk_struct || con->type->kind == tk_class)
                                 ? con->type->variant.class_struct_union.field_list
                                 : nullptr;
    a_constant_ptr c = con->variant.aggregate.first_constant;

    if (field_head != nullptr) {
      std::vector<llvm::Constant*> elems;
      unsigned i = 0;
      a_field_ptr field = field_head;

      while (i < st_ty->getNumElements() && (c != nullptr || field != nullptr)) {
#if DO_IL_LOWERING
        while (c != nullptr && c->initializes_empty_object) {
          c = c->next;
        }
#endif
        while (c != nullptr && c->kind == ck_designator) {
          a_field_ptr target_field = nullptr;
          if (!c->variant.designator.is_generic) {
            target_field = c->variant.designator.variant.field;
          } else if (c->variant.designator.variant.field_name != nullptr) {
            for (a_field_ptr scan = field_head; scan != nullptr; scan = scan->next) {
              if (scan->source_corresp.name != nullptr &&
                  std::strcmp(scan->source_corresp.name,
                              c->variant.designator.variant.field_name) == 0) {
                target_field = scan;
                break;
              }
            }
          }
          c = c->next;
          while (target_field != nullptr && field != target_field &&
                 i < st_ty->getNumElements()) {
            elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
            field = field->next;
            i++;
          }
          if (i >= st_ty->getNumElements()) {
            break;
          }
        }
        if (i >= st_ty->getNumElements()) {
          break;
        }

        llvm::Type* elem_ty = st_ty->getElementType(i);
        bool is_synthetic_pad = false;
        if (elem_ty->isArrayTy() && elem_ty->getArrayElementType()->isIntegerTy(8)) {
          if (!field || skip_typerefs(field->type)->kind != tk_array) {
            is_synthetic_pad = true;
          }
        }
        if (is_synthetic_pad) {
          elems.push_back(llvm::Constant::getNullValue(elem_ty));
          i++;
          continue;
        }
        if (field && field->is_bit_field) {
          llvm::Constant* packed_bf = nullptr;
          LLVM_GEN_BE_ASSERT_OK(llvm_pack_constant_bitfield_bytes(&field, &c, elem_ty, &packed_bf));
          elems.push_back(packed_bf);
          i++;
          continue;
        }
        if (c != nullptr) {
          if (c->kind == ck_init_repeat) {
            uint64_t count = c->variant.init_repeat.count;
            a_constant_ptr rep_src = c->variant.init_repeat.constant;
            c = c->next;
            for (uint64_t r = 0; r < count && i < st_ty->getNumElements(); ++r) {
              llvm::Constant* rep_const = nullptr;
              llvm_gen_be_error_t err = evaluate_constant(rep_src, st_ty->getElementType(i), &rep_const);
              if (err != llvm_gen_be_error_t::ok) return err;
              elems.push_back(rep_const);
              if (field) field = field->next;
              i++;
            }
            continue;
          }
          llvm::Constant* elem_const = nullptr;
          llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
          if (err != llvm_gen_be_error_t::ok) return err;
          elems.push_back(elem_const);
          c = c->next;
        } else {
          elems.push_back(llvm::Constant::getNullValue(elem_ty));
        }
        if (field) field = field->next;
        i++;
      }
      while (i < st_ty->getNumElements()) {
        elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
        i++;
      }
      *out_const = llvm::ConstantStruct::get(st_ty, elems);
      return llvm_gen_be_error_t::ok;
    }

    std::vector<llvm::Constant*> elems;
    unsigned i = 0;
    for (; c != nullptr; c = c->next) {
      if (i < st_ty->getNumElements()) {
        llvm::Constant* elem_const = nullptr;
        llvm_gen_be_error_t err = evaluate_constant(c, st_ty->getElementType(i), &elem_const);
        if (err != llvm_gen_be_error_t::ok) return err;
        elems.push_back(elem_const);
        i++;
      }
    }
    while (i < st_ty->getNumElements()) {
      elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
      i++;
    }
    *out_const = llvm::ConstantStruct::get(st_ty, elems);
    return llvm_gen_be_error_t::ok;
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Dispatches an EDG constant node (`a_constant_ptr`) to the appropriate LLVM constant builder.
 * @details Supports integer, float, complex, imaginary, string, address, aggregate, pointer-to-member,
 *          label-difference, stack-offset, dynamic-init, and void constants.
 * @param[in] con Pointer to the EDG constant node (or nullptr for zero-initialization).
 * @param[in] expected_ty The expected LLVM type of the resulting constant.
 * @param[out] out_const Receives the evaluated `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t evaluate_constant(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (!con) {
    *out_const = llvm::Constant::getNullValue(expected_ty);
    return llvm_gen_be_error_t::ok;
  }

  switch (con->kind) {
    case ck_integer:
      return llvm_const_from_integer(con, expected_ty, out_const);
#if FIXED_POINT_ALLOWED
    case ck_fixed_point: {
      llvm::Type* fx_ty = expected_ty;
      if (!fx_ty || !fx_ty->isIntegerTy()) {
        LLVM_GEN_BE_CHECK(llvm_type_from_edg_type(con->type, &fx_ty));
      }
      *out_const = llvm::ConstantInt::get(
          fx_ty, static_cast<uint64_t>(con->variant.fixed_point_value));
      return llvm_gen_be_error_t::ok;
    }
#endif
    case ck_float:
      return llvm_const_from_float(con, expected_ty, out_const);
    case ck_complex:
      return llvm_const_from_complex(con, expected_ty, out_const);
    case ck_imaginary:
      return llvm_const_from_imaginary(con, expected_ty, out_const);
    case ck_string:
      return llvm_const_from_string(con, expected_ty, out_const);
    case ck_address:
      return llvm_const_from_address(con, expected_ty, out_const);
    case ck_aggregate:
      return llvm_const_from_aggregate(con, expected_ty, out_const);
    case ck_ptr_to_member:
      return llvm_const_from_ptr_to_member(con, expected_ty, out_const);
    case ck_label_difference:
      return llvm_const_from_label_difference(con, expected_ty, out_const);
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      return llvm_const_from_stack_offset(con, expected_ty, out_const);
#endif
    case ck_dynamic_init:
      return llvm_const_from_dynamic_init(con, expected_ty, out_const);
    case ck_template_param:
      return llvm_const_from_template_param(con, expected_ty, out_const);
    case ck_void:
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    default:
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::unsupported_expr;
  }
}

END_EDG_NAMESPACE
#endif
