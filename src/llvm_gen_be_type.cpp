/**
 * @file llvm_gen_be_type.cpp
 * @brief Implementation of type translation and lowering for LLVM backend.
 * @details Translates EDG types into LLVM types, returning llvm_gen_be_error_t
 * and propagating error codes on failure.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_type.h"
#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "target.h"
#include <vector>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG integer type into a matching LLVM IntegerType.
 * @details Maps integer kinds (char, short, int, long, long long, __int128, bool)
 * into appropriate bit-width LLVM integer types.
 * @param[in] edg_type Pointer to the EDG integer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_integer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (is_bool_type(edg_type)) {
    *out_type = llvm::Type::getInt1Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  switch (edg_type->variant.integer.int_kind) {
    case ik_char:
    case ik_signed_char:
    case ik_unsigned_char:
      *out_type = llvm::IntegerType::get(*be_state->context, targ_char_bit);
      break;
    case ik_short:
    case ik_unsigned_short:
      *out_type = llvm::IntegerType::get(*be_state->context, (targ_sizeof_short * targ_char_bit));
      break;
    case ik_int:
    case ik_unsigned_int:
      *out_type = llvm::IntegerType::get(*be_state->context, (targ_sizeof_int * targ_char_bit));
      break;
    case ik_long:
    case ik_unsigned_long:
      *out_type = llvm::IntegerType::get(*be_state->context, (targ_sizeof_long * targ_char_bit));
      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:
    case ik_unsigned_long_long:
      *out_type = llvm::IntegerType::get(*be_state->context, (targ_sizeof_long_long * targ_char_bit));
      break;
#endif /* LONG_LONG_ALLOWED */
    default:
      *out_type = llvm::IntegerType::get(*be_state->context, edg_type->size * targ_char_bit);
      break;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG floating-point type into a matching LLVM floating-point Type.
 * @details Handles half/fp16, bfloat16, float, double, x86_fp80, and fp128.
 * @param[in] edg_type Pointer to the EDG floating-point type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_float(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::Type::getDoubleTy(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  switch (edg_type->variant.float_kind) {
    case fk_float16:
    case fk_fp16:
    case fk_std_float16:
      *out_type = llvm::Type::getHalfTy(*be_state->context);
      break;
    case fk_std_bfloat16:
      *out_type = llvm::Type::getBFloatTy(*be_state->context);
      break;
    case fk_float:
    case fk_std_float32:
    case fk_float32x:
      *out_type = llvm::Type::getFloatTy(*be_state->context);
      break;
    case fk_double:
    case fk_std_float64:
    case fk_float64x:
      *out_type = llvm::Type::getDoubleTy(*be_state->context);
      break;
    case fk_float80:
      *out_type = llvm::Type::getX86_FP80Ty(*be_state->context);
      break;
    case fk_float128:
    case fk_std_float128:
      *out_type = llvm::Type::getFP128Ty(*be_state->context);
      break;
    case fk_long_double:
      if (edg_type->size * targ_char_bit == 80) {
        *out_type = llvm::Type::getX86_FP80Ty(*be_state->context);
      } else if (edg_type->size * targ_char_bit == 128) {
        *out_type = llvm::Type::getFP128Ty(*be_state->context);
      } else {
        *out_type = llvm::Type::getDoubleTy(*be_state->context);
      }
      break;
    default:
      *out_type = llvm::Type::getDoubleTy(*be_state->context);
      return llvm_gen_be_error_t::unsupported_type;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG pointer or reference type into an LLVM opaque PointerType.
 * @details Constructs an opaque pointer using LLVM's unadorned pointer representation.
 * @param[in] edg_type Pointer to the EDG pointer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_pointer(
    [[maybe_unused]] a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  *out_type = llvm::PointerType::getUnqual(*be_state->context);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG array type into an LLVM ArrayType or PointerType.
 * @details Maps fixed-size arrays to llvm::ArrayType, zero-bound arrays to 0-length arrays,
 * and variable-length or incomplete arrays to opaque pointers.
 * @param[in] edg_type Pointer to the EDG array type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_array(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::PointerType::getUnqual(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (!edg_type->variant.array.is_vla && !edg_type->variant.array.bound_is_zero &&
      !edg_type->variant.array.is_variable_size_array &&
      !edg_type->variant.array.is_template_dependent_size_array) {
    llvm::Type* elem_ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type->variant.array.element_type, &elem_ty);
    if (err != llvm_gen_be_error_t::ok) {
      return err;
    }
    *out_type = llvm::ArrayType::get(elem_ty, edg_type->variant.array.variant.number_of_elements);
  } else if (edg_type->variant.array.bound_is_zero) {
    llvm::Type* elem_ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type->variant.array.element_type, &elem_ty);
    if (err != llvm_gen_be_error_t::ok) {
      return err;
    }
    *out_type = llvm::ArrayType::get(elem_ty, 0);
  } else {
    *out_type = llvm::PointerType::getUnqual(*be_state->context);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG struct, union, or class type into an LLVM StructType.
 * @details Generates packed structures matching EDG field byte offsets and tail padding.
 * Cyclic struct references are resolved using forward-declared opaque StructTypes.
 * @param[in] edg_type Pointer to the EDG struct, union, or class type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_struct(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::StructType::get(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (edg_type->kind == tk_union) {
    llvm::StructType* struct_ty = llvm::StructType::create(*be_state->context);
    be_state->type_cache[edg_type] = struct_ty;

    std::vector<llvm::Type*> elem_tys;
    llvm::Type* largest_elem = nullptr;
    uint64_t max_size = 0;
    for (a_field_ptr field = edg_type->variant.class_struct_union.field_list;
         field != nullptr; field = field->next) {
      if (field->type->size > max_size) {
        max_size = field->type->size;
        llvm_gen_be_error_t err = llvm_type_from_edg_type(field->type, &largest_elem);
        if (err != llvm_gen_be_error_t::ok) {
          return err;
        }
      }
    }
    if (largest_elem != nullptr) {
      elem_tys.push_back(largest_elem);
      if (max_size < static_cast<uint64_t>(edg_type->size)) {
        elem_tys.push_back(llvm::ArrayType::get(
            llvm::Type::getInt8Ty(*be_state->context),
            edg_type->size - max_size));
      }
    } else if (edg_type->size > 0) {
      elem_tys.push_back(llvm::ArrayType::get(
          llvm::Type::getInt8Ty(*be_state->context),
          edg_type->size));
    }
    struct_ty->setBody(elem_tys, /*isPacked=*/false);
    *out_type = struct_ty;
    return llvm_gen_be_error_t::ok;
  }

  /* Struct / Class */
  llvm::StructType* struct_ty = llvm::StructType::create(*be_state->context);
  be_state->type_cache[edg_type] = struct_ty;

  std::vector<llvm::Type*> elem_tys;
  uint64_t current_offset = 0;
  for (a_field_ptr field = edg_type->variant.class_struct_union.field_list;
       field != nullptr; field = field->next) {
    if (field->offset > current_offset) {
      elem_tys.push_back(llvm::ArrayType::get(
          llvm::Type::getInt8Ty(*be_state->context),
          field->offset - current_offset));
      current_offset = field->offset;
    } else if (field->offset < current_offset) {
      continue;
    }

    if (field->is_bit_field && field->bit_size == 0) {
      continue;
    }

    llvm::Type* ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(field->type, &ty);
    if (err != llvm_gen_be_error_t::ok) {
      return err;
    }
    elem_tys.push_back(ty);
    current_offset += field->type->size;
  }

  if (current_offset < static_cast<uint64_t>(edg_type->size)) {
    elem_tys.push_back(llvm::ArrayType::get(
        llvm::Type::getInt8Ty(*be_state->context),
        edg_type->size - current_offset));
  }

  if (elem_tys.empty() && edg_type->size > 0) {
    elem_tys.push_back(llvm::ArrayType::get(
        llvm::Type::getInt8Ty(*be_state->context),
        edg_type->size));
  }

  struct_ty->setBody(elem_tys, /*isPacked=*/true);
  *out_type = struct_ty;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG routine (function) type into an LLVM FunctionType.
 * @details Gathers return type, parameter types, and variadic ellipsis flag.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_fn_type Pointer to the variable where the resulting FunctionType* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_routine(
    a_type_ptr edg_type,
    llvm::FunctionType** out_fn_type) noexcept {
  if (out_fn_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_fn_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
    return llvm_gen_be_error_t::ok;
  }

  a_type_ptr ret_ty = edg_type->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  std::vector<llvm::Type*> param_tys;
  if (edg_type->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = edg_type->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      llvm::Type* pty = nullptr;
      err = llvm_type_from_edg_type(param->type, &pty);
      if (err != llvm_gen_be_error_t::ok) {
        return err;
      }
      param_tys.push_back(pty);
    }
  }

  bool is_vararg = (edg_type->variant.routine.extra_info != nullptr)
                       ? edg_type->variant.routine.extra_info->has_ellipsis
                       : false;

  *out_fn_type = llvm::FunctionType::get(llvm_ret_ty, param_tys, is_vararg);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG type representation into a corresponding LLVM type.
 * @details Traverses the EDG type structure, looks up or populates the type cache,
 * and creates LLVM primitive, pointer, array, aggregate, or routine types.
 * @param[in] edg_type Pointer to the EDG type description structure. If null, produces void.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, llvm_gen_be_error_t::invalid_argument if
 *         out_type is null, or an error code indicating the failure reason.
 */
llvm_gen_be_error_t llvm_type_from_edg_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::Type::getVoidTy(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  auto it = be_state->type_cache.find(edg_type);
  if (it != be_state->type_cache.end()) {
    *out_type = it->second;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  switch (edg_type->kind) {
    case tk_void:
      llvm_ty = llvm::Type::getVoidTy(*be_state->context);
      break;
    case tk_integer:
      err = llvm_type_from_integer(edg_type, &llvm_ty);
      break;
    case tk_float:
      err = llvm_type_from_float(edg_type, &llvm_ty);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex: {
      llvm::Type* elem_ty = nullptr;
      err = llvm_type_from_float(edg_type, &elem_ty);
      if (err == llvm_gen_be_error_t::ok) {
        llvm_ty = llvm::StructType::get(*be_state->context, {elem_ty, elem_ty});
      }
      break;
    }
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_pointer:
      err = llvm_type_from_pointer(edg_type, &llvm_ty);
      break;
    case tk_array:
      err = llvm_type_from_array(edg_type, &llvm_ty);
      break;
    case tk_ptr_to_member: {
      a_type_ptr mem_ty = edg_type->variant.ptr_to_member.type;
      if (mem_ty != nullptr && mem_ty->kind == tk_routine) {
        llvm::Type* int_ty = llvm::IntegerType::get(*be_state->context, targ_sizeof_pointer * targ_char_bit);
        llvm_ty = llvm::StructType::get(*be_state->context, {int_ty, int_ty});
      } else {
        llvm_ty = llvm::IntegerType::get(*be_state->context, targ_sizeof_pointer * targ_char_bit);
      }
      break;
    }
    case tk_routine: {
      llvm::FunctionType* fn_ty = nullptr;
      err = llvm_type_from_routine(edg_type, &fn_ty);
      llvm_ty = fn_ty;
      break;
    }
    case tk_struct:
    case tk_class:
    case tk_union:
      err = llvm_type_from_struct(edg_type, &llvm_ty);
      break;
    case tk_typeref:
      err = llvm_type_from_edg_type(edg_type->variant.typeref.type, &llvm_ty);
      break;
    default:
      llvm_ty = llvm::Type::getInt8Ty(*be_state->context);
      err = llvm_gen_be_error_t::unsupported_type;
      break;
  }

  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  be_state->type_cache[edg_type] = llvm_ty;
  *out_type = llvm_ty;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Legacy compatibility helper redirecting to llvm_type_from_edg_type.
 * @details Convenience wrapper returning llvm::Type* while inspecting error codes internally.
 * @param[in] edg_type Pointer to the EDG type structure.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Type* ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type, &ty);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }
  *out_type = ty;
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
