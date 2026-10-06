#include "basic_hdrs.h"
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
      // Simplified: return 0.0 for now since internal format translation is complex
      return llvm::ConstantFP::get(expected_ty, 0.0);
    }
    case ck_string: {
      a_targ_size_t len = con->variant.string.length;
      const char* chars = con->variant.string.value;
      llvm::StringRef str(chars, len);
      // Determine element type size (could be wchar_t)
      // For now, simplify by creating an i8 array. 
      llvm::Constant* arr = llvm::ConstantDataArray::getString(*be_state->context, str, false);
      if (expected_ty->isPointerTy()) {
        llvm::GlobalVariable* gv = new llvm::GlobalVariable(
            *be_state->module, arr->getType(), true,
            llvm::GlobalValue::PrivateLinkage, arr, ".str");
        return gv;
      }
      return arr;
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
