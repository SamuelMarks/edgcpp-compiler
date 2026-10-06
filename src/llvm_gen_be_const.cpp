#include "basic_hdrs.h"
#include "float_pt.h"
#include "llvm_gen_be_internal.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm::Constant* evaluate_constant(a_constant_ptr con, llvm::Type* expected_ty) {
  if (!con) return llvm::Constant::getNullValue(expected_ty);
  switch (con->kind) {
    case ck_integer: {
      bool is_unsigned = (con->type->kind == tk_integer || con->type->kind == tk_enum) ? 
                           !int_kind_is_signed[con->type->variant.integer.int_kind] : false;
      uint64_t val = (uint64_t)con->variant.integer_value;
      return llvm::ConstantInt::get(expected_ty, val, !is_unsigned);
    }
    case ck_float: {
      a_boolean pos_inf = FALSE, neg_inf = FALSE, nan = FALSE;
      a_float_kind fk = con->type->variant.float_kind;
      a_number_buffer hex_str = fp_to_hex_constant_string(
          fk, &con->variant.float_value, &pos_inf, &neg_inf, &nan);

      if (pos_inf) return llvm::ConstantFP::getInfinity(expected_ty, false);
      if (neg_inf) return llvm::ConstantFP::getInfinity(expected_ty, true);
      if (nan) return llvm::ConstantFP::getQNaN(expected_ty);

      llvm::StringRef str_ref(hex_str.c_str());
      return llvm::ConstantFP::get(expected_ty, str_ref);
    }
    case ck_string: {
      a_targ_size_t len = con->variant.string.length;
      const char* chars = con->variant.string.value;
      llvm::StringRef str(chars, len);

      llvm::Type* actual_array_ty = get_llvm_type(con->type);
      llvm::Type* elem_ty = nullptr;
      if (actual_array_ty->isArrayTy()) {
        elem_ty = actual_array_ty->getArrayElementType();
      } else {
        f_fatal(con->source_corresp.position, "String constant type is not an array");
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
        arr = llvm::ConstantDataArray::get(*be_state->context, llvm::ArrayRef<uint8_t>(reinterpret_cast<const uint8_t*>(chars), len));
      }

      if (expected_ty->isPointerTy()) {
        llvm::GlobalVariable* gv = new llvm::GlobalVariable(
            *be_state->module, arr->getType(), true,
            llvm::GlobalValue::PrivateLinkage, arr, ".str");
        return gv;
      }
      return arr;
    }
    case ck_address: {
      if (con->variant.address.kind == abk_label) {
        a_label_ptr label = con->variant.address.variant.label;
        llvm::BasicBlock* label_bb = nullptr;
        if (be_state->label_blocks.count(label)) {
          label_bb = be_state->label_blocks[label];
        } else {
          label_bb = llvm::BasicBlock::Create(*be_state->context, "label");
          be_state->label_blocks[label] = label_bb;
        }
        llvm::Function* func = label_bb->getParent();
        if (!func) {
          func = be_state->builder->GetInsertBlock()->getParent();
        }
        return llvm::BlockAddress::get(func, label_bb);
      }
      return llvm::Constant::getNullValue(expected_ty);
    }
    case ck_aggregate: {
      if (expected_ty->isArrayTy()) {
        std::vector<llvm::Constant*> elems;
        llvm::Type* elem_ty = expected_ty->getArrayElementType();
        for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
          elems.push_back(evaluate_constant(c, elem_ty));
        }
        unsigned expected_elems = expected_ty->getArrayNumElements();
        while (elems.size() < expected_elems) {
           elems.push_back(llvm::Constant::getNullValue(elem_ty));
        }
        return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(expected_ty), elems);
      } else if (expected_ty->isStructTy()) {
        std::vector<llvm::Constant*> elems;
        llvm::StructType* st_ty = llvm::cast<llvm::StructType>(expected_ty);
        unsigned i = 0;
        for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
          if (i < st_ty->getNumElements()) {
             elems.push_back(evaluate_constant(c, st_ty->getElementType(i)));
             i++;
          }
        }
        while (i < st_ty->getNumElements()) {
           elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
           i++;
        }
        return llvm::ConstantStruct::get(st_ty, elems);
      }
      return llvm::Constant::getNullValue(expected_ty);
    }
    default:
      return llvm::Constant::getNullValue(expected_ty);
  }
}

END_EDG_NAMESPACE
#endif
