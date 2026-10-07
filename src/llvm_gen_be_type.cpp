#include <type_traits>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm::Type* get_llvm_type(a_type_ptr edg_type) {
  if (!edg_type) return llvm::Type::getVoidTy(*be_state->context);

  auto it = be_state->type_cache.find(edg_type);
  if (it != be_state->type_cache.end()) {
    return it->second;
  }

  llvm::Type* llvm_ty = nullptr;

  switch (edg_type->kind) {
    case tk_void:
      llvm_ty = llvm::Type::getVoidTy(*be_state->context);
      break;
    case tk_integer: {
      if (is_bool_type(edg_type)) {
        llvm_ty = llvm::Type::getInt1Ty(*be_state->context);
        break;
      }
      switch (edg_type->variant.integer.int_kind) {
        case ik_char:
        case ik_signed_char:
        case ik_unsigned_char:
          llvm_ty = llvm::IntegerType::get(*be_state->context, targ_char_bit);
          break;
        case ik_short:
        case ik_unsigned_short:
          llvm_ty = llvm::IntegerType::get(*be_state->context, (targ_sizeof_short * targ_char_bit));
          break;
        case ik_int:
        case ik_unsigned_int:
          llvm_ty = llvm::IntegerType::get(*be_state->context, (targ_sizeof_int * targ_char_bit));
          break;
        case ik_long:
        case ik_unsigned_long:
          llvm_ty = llvm::IntegerType::get(*be_state->context, (targ_sizeof_long * targ_char_bit));
          break;
#if LONG_LONG_ALLOWED
        case ik_long_long:
        case ik_unsigned_long_long:
          llvm_ty = llvm::IntegerType::get(*be_state->context, (targ_sizeof_long_long * targ_char_bit));
          break;
#endif /* LONG_LONG_ALLOWED */
        default:
          llvm_ty = llvm::IntegerType::get(*be_state->context, edg_type->size * targ_char_bit);
          break;
      }
      break;
    }
    case tk_float: {
      switch (edg_type->variant.float_kind) {
        case fk_float16:
        case fk_fp16:
        case fk_std_float16:
          llvm_ty = llvm::Type::getHalfTy(*be_state->context);
          break;
        case fk_std_bfloat16:
          llvm_ty = llvm::Type::getBFloatTy(*be_state->context);
          break;
        case fk_float:
        case fk_std_float32:
        case fk_float32x:
          llvm_ty = llvm::Type::getFloatTy(*be_state->context);
          break;
        case fk_double:
        case fk_std_float64:
        case fk_float64x:
          llvm_ty = llvm::Type::getDoubleTy(*be_state->context);
          break;
        case fk_float80:
          llvm_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
          break;
        case fk_float128:
        case fk_std_float128:
          llvm_ty = llvm::Type::getFP128Ty(*be_state->context);
          break;
        case fk_long_double:
          // Depending on target, long double might be fp80, double, or fp128.
          // For most systems (like x86 Linux), it's fp80.
          if (edg_type->size * targ_char_bit == 80) {
            llvm_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
          } else if (edg_type->size * targ_char_bit == 128) {
            llvm_ty = llvm::Type::getFP128Ty(*be_state->context);
          } else {
            llvm_ty = llvm::Type::getDoubleTy(*be_state->context);
          }
          break;
        default:
          internal_error("Unsupported floating-point kind in LLVM backend");
          llvm_ty = llvm::Type::getDoubleTy(*be_state->context);
          break;
      }
      break;
    }
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex: {
      llvm::Type* elem_ty = nullptr;
      switch (edg_type->variant.float_kind) {
        case fk_float16:
        case fk_fp16:
        case fk_std_float16:
          elem_ty = llvm::Type::getHalfTy(*be_state->context);
          break;
        case fk_std_bfloat16:
          elem_ty = llvm::Type::getBFloatTy(*be_state->context);
          break;
        case fk_float:
        case fk_std_float32:
        case fk_float32x:
          elem_ty = llvm::Type::getFloatTy(*be_state->context);
          break;
        case fk_double:
        case fk_std_float64:
        case fk_float64x:
          elem_ty = llvm::Type::getDoubleTy(*be_state->context);
          break;
        case fk_float80:
          elem_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
          break;
        case fk_float128:
        case fk_std_float128:
          elem_ty = llvm::Type::getFP128Ty(*be_state->context);
          break;
        case fk_long_double:
          if (edg_type->size * targ_char_bit == 160) {
            elem_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
          } else if (edg_type->size * targ_char_bit == 256) {
            elem_ty = llvm::Type::getFP128Ty(*be_state->context);
          } else {
            elem_ty = llvm::Type::getDoubleTy(*be_state->context);
          }
          break;
        default:
          internal_error("Unsupported complex floating-point kind in LLVM backend");
          elem_ty = llvm::Type::getDoubleTy(*be_state->context);
          break;
      }
      llvm_ty = llvm::StructType::get(*be_state->context, {elem_ty, elem_ty});
      break;
    }
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_pointer:
      // Opaque pointers in modern LLVM
      llvm_ty = llvm::PointerType::getUnqual(*be_state->context);
      break;
    case tk_array:
      if (!edg_type->variant.array.is_vla && !edg_type->variant.array.bound_is_zero && !edg_type->variant.array.is_variable_size_array && !edg_type->variant.array.is_template_dependent_size_array) {
        llvm_ty = llvm::ArrayType::get(get_llvm_type(edg_type->variant.array.element_type), edg_type->variant.array.variant.number_of_elements);
      } else if (edg_type->variant.array.bound_is_zero) {
        llvm_ty = llvm::ArrayType::get(get_llvm_type(edg_type->variant.array.element_type), 0);
      } else {
        // VLA or incomplete array
        llvm_ty = llvm::PointerType::getUnqual(*be_state->context);
      }
      break;
    case tk_ptr_to_member: {
      a_type_ptr mem_ty = edg_type->variant.ptr_to_member.type;
      if (mem_ty->kind == tk_routine) {
        // Pointer to member function ({ptrdiff_t, ptrdiff_t})
        llvm::Type* int_ty = llvm::IntegerType::get(*be_state->context, targ_sizeof_pointer * targ_char_bit);
        llvm_ty = llvm::StructType::get(*be_state->context, {int_ty, int_ty});
      } else {
        // Pointer to data member (ptrdiff_t)
        llvm_ty = llvm::IntegerType::get(*be_state->context, targ_sizeof_pointer * targ_char_bit);
      }
      break;
    }
    case tk_routine: {
      a_type_ptr ret_ty = edg_type->variant.routine.return_type;
      llvm::Type* llvm_ret_ty = get_llvm_type(ret_ty);
      std::vector<llvm::Type*> param_tys;
      if (edg_type->variant.routine.extra_info) {
        for (a_param_type_ptr param = edg_type->variant.routine.extra_info->param_type_list;
             param != nullptr; param = param->next) {
          param_tys.push_back(get_llvm_type(param->type));
        }
      }
      bool is_vararg = edg_type->variant.routine.extra_info ? edg_type->variant.routine.extra_info->has_ellipsis : false;
      llvm_ty = llvm::FunctionType::get(llvm_ret_ty, param_tys, is_vararg);
      break;
    }
    case tk_struct:
    case tk_class: {
      // Forward declaration caching to avoid cycles
      llvm::StructType* struct_ty = llvm::StructType::create(*be_state->context);
      be_state->type_cache[edg_type] = struct_ty;
      
      std::vector<llvm::Type*> elem_tys;
      uint64_t current_offset = 0;
      for (a_field_ptr field = edg_type->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
        if (field->offset > current_offset) {
          // Add explicit padding
          elem_tys.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), field->offset - current_offset));
          current_offset = field->offset;
        } else if (field->offset < current_offset) {
          // Overlapping bitfield, already covered by the previous allocation
          continue;
        }

        if (field->is_bit_field && field->bit_size == 0) continue; // Zero-length bitfield

        llvm::Type* ty = get_llvm_type(field->type);
        elem_tys.push_back(ty);
        current_offset += field->type->size;
      }
      
      // Add tail padding
      if (current_offset < edg_type->size) {
        elem_tys.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), edg_type->size - current_offset));
      }
      
      if (elem_tys.empty() && edg_type->size > 0) {
        elem_tys.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), edg_type->size));
      }
      
      // Set to packed so LLVM doesn't add its own padding
      struct_ty->setBody(elem_tys, /*isPacked=*/true);
      llvm_ty = struct_ty;
      break;
    }
    case tk_union: {
      llvm::StructType* struct_ty = llvm::StructType::create(*be_state->context);
      be_state->type_cache[edg_type] = struct_ty;
      
      std::vector<llvm::Type*> elem_tys;
      llvm::Type* largest_elem = nullptr;
      uint64_t max_size = 0;
      for (a_field_ptr field = edg_type->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
        if (field->type->size > max_size) {
          max_size = field->type->size;
          largest_elem = get_llvm_type(field->type);
        }
      }
      if (largest_elem) {
        elem_tys.push_back(largest_elem);
        if (max_size < (uint64_t)edg_type->size) {
          elem_tys.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), edg_type->size - max_size));
        }
      } else if (edg_type->size > 0) {
        elem_tys.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), edg_type->size));
      }
      struct_ty->setBody(elem_tys, /*isPacked=*/false);
      llvm_ty = struct_ty;
      break;
    }
    
    case tk_typeref:
      llvm_ty = get_llvm_type(edg_type->variant.typeref.type);
      break;
    default:
      internal_error("Unhandled AST type kind encountered in LLVM backend");
      llvm_ty = llvm::Type::getInt8Ty(*be_state->context);
      break;
  }

  be_state->type_cache[edg_type] = llvm_ty;
  return llvm_ty;
}

END_EDG_NAMESPACE
#endif
