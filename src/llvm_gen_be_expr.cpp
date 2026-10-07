#include <type_traits>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "target.h"
#include "lower_name.h"


#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm::Constant* get_typeinfo_global(a_type_ptr type) {
  if (!type) {
    return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
  }
  
  // Use EDG mangler to get the typeinfo name
  char* mangled = mangled_typeinfo_name(type);
  llvm::StringRef name_ref(mangled);
  
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (!gv) {
      gv = new llvm::GlobalVariable(
          *be_state->module,
          llvm::PointerType::getUnqual(*be_state->context),
          true,
          llvm::GlobalValue::ExternalLinkage,
          nullptr,
          name_ref
      );
  }
  return gv;
}

llvm::FunctionType* get_llvm_function_type(a_type_ptr ty) {
  while (ty) {
    if (ty->kind == tk_pointer || is_reference_type(ty)) {
      ty = ty->variant.pointer.type;
    } else if (ty->kind == tk_typeref) {
      ty = ty->variant.typeref.type;
    } else {
      break;
    }
  }
  if (ty && ty->kind == tk_routine) {
    llvm::Type* lt = get_llvm_type(ty);
    if (lt->isFunctionTy()) return llvm::cast<llvm::FunctionType>(lt);
  }
  return nullptr;
}

llvm::Value* emit_expression(an_expr_node_ptr expr) {
  if (!expr) return nullptr;
  llvm::errs() << "emit_expression kind: " << expr->kind << "\n";
  if (!expr) return nullptr;

  switch (expr->kind) {
    case enk_constant: {
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) return llvm::Constant::getNullValue(get_llvm_type(expr->type));
       return evaluate_constant(con, get_llvm_type(expr->type));
    }
    case enk_routine: {
       a_routine_ptr routine = expr->variant.routine.ptr;
       if (!routine || !routine->source_corresp.name) return llvm::Constant::getNullValue(get_llvm_type(expr->type));
       llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
       if (!func) {
         llvm::Type* func_ty = get_llvm_type(routine->type);
         func = llvm::Function::Create(
            llvm::cast<llvm::FunctionType>(func_ty),
            llvm::GlobalValue::ExternalLinkage,
            routine->source_corresp.name,
            be_state->module.get()
         );
       }
       if (!expr->is_lvalue) {
         // Functions are fundamentally rvalues as pointers in LLVM but typically represented as pointers.
         // Wait, in EDG, routines decay to pointers if not lvalue? Let's just return the function.
         return func;
       }
       return func;
    }
    case enk_variable: {
       a_variable_ptr var = expr->variant.variable.ptr;
       llvm::Value* ptr = nullptr;
       auto it = be_state->local_vars.find(var);
       if (it != be_state->local_vars.end()) {
         ptr = it->second;
       } else {
         const char* name = var->source_corresp.name ? var->source_corresp.name : "";
         ptr = be_state->module->getNamedGlobal(name);
         if (!ptr && name[0] != '\0') {
           // Auto-generate missing global reference (e.g. for undeclared externs or phase ordering gaps)
           llvm::Type* var_ty = get_llvm_type(expr->type);
           ptr = new llvm::GlobalVariable(
             *be_state->module,
             var_ty,
             false, // isConstant
             llvm::GlobalValue::ExternalLinkage,
             nullptr,
             name
           );
         }
       }
       if (!ptr) {
         internal_error("Unresolved variable encountered in LLVM backend");
         return llvm::Constant::getNullValue(get_llvm_type(expr->type));
       }
       if (!expr->is_lvalue) {
         // LValue-to-RValue implicit conversion
         llvm::LoadInst* load = be_state->builder->CreateLoad(get_llvm_type(expr->type), ptr);
         if (is_volatile_qualified_type(expr->type)) {
           load->setVolatile(true);
         }
         return load;
       }
       return ptr;
    }
    case enk_operation: {
       an_expr_node_ptr op1 = expr->variant.operation.operands;
       an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
       llvm::Value* v1 = op1 ? emit_expression(op1) : nullptr;
       llvm::Value* v2 = op2 ? emit_expression(op2) : nullptr;
       
       switch (expr->variant.operation.kind) {
         case eok_add:
           if (v1 && v2) {
             if (v1->getType()->isFloatingPointTy()) return be_state->builder->CreateFAdd(v1, v2);
             return be_state->builder->CreateAdd(v1, v2);
           }
           break;
         case eok_subtract:
           if (v1 && v2) {
             if (v1->getType()->isFloatingPointTy()) return be_state->builder->CreateFSub(v1, v2);
             return be_state->builder->CreateSub(v1, v2);
           }
           break;
         case eok_multiply:
           if (v1 && v2) {
             if (v1->getType()->isFloatingPointTy()) return be_state->builder->CreateFMul(v1, v2);
             return be_state->builder->CreateMul(v1, v2);
           }
           break;
         case eok_divide:
           if (v1 && v2) {
             if (v1->getType()->isFloatingPointTy()) return be_state->builder->CreateFDiv(v1, v2);
             bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
             if (is_unsigned) return be_state->builder->CreateUDiv(v1, v2);
             return be_state->builder->CreateSDiv(v1, v2);
           }
           break;
         case eok_remainder:
           if (v1 && v2) {
             if (v1->getType()->isFloatingPointTy()) return be_state->builder->CreateFRem(v1, v2);
             bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
             if (is_unsigned) return be_state->builder->CreateURem(v1, v2);
             return be_state->builder->CreateSRem(v1, v2);
           }
           break;
         case eok_and:
           if (v1 && v2) return be_state->builder->CreateAnd(v1, v2);
           break;
         case eok_or:
           if (v1 && v2) return be_state->builder->CreateOr(v1, v2);
           break;
         case eok_xor:
           if (v1 && v2) return be_state->builder->CreateXor(v1, v2);
           break;
         case eok_shiftl:
           if (v1 && v2) return be_state->builder->CreateShl(v1, v2);
           break;
         case eok_shiftr:
           if (v1 && v2) {
             bool is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
             if (is_unsigned) return be_state->builder->CreateLShr(v1, v2);
             return be_state->builder->CreateAShr(v1, v2);
           }
           break;
         case eok_eq:
         case eok_ne:
         case eok_lt:
         case eok_le:
         case eok_gt:
         case eok_ge:
           if (v1 && v2) {
             bool is_fp = v1->getType()->isFloatingPointTy();
             bool is_unsigned = !is_fp && (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
             llvm::CmpInst::Predicate pred;
             switch (expr->variant.operation.kind) {
                case eok_eq: pred = is_fp ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ; break;
                case eok_ne: pred = is_fp ? llvm::CmpInst::FCMP_ONE : llvm::CmpInst::ICMP_NE; break;
                case eok_lt: pred = is_fp ? llvm::CmpInst::FCMP_OLT : (is_unsigned ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT); break;
                case eok_le: pred = is_fp ? llvm::CmpInst::FCMP_OLE : (is_unsigned ? llvm::CmpInst::ICMP_ULE : llvm::CmpInst::ICMP_SLE); break;
                case eok_gt: pred = is_fp ? llvm::CmpInst::FCMP_OGT : (is_unsigned ? llvm::CmpInst::ICMP_UGT : llvm::CmpInst::ICMP_SGT); break;
                case eok_ge: pred = is_fp ? llvm::CmpInst::FCMP_OGE : (is_unsigned ? llvm::CmpInst::ICMP_UGE : llvm::CmpInst::ICMP_SGE); break;
                default: break;
             }
             if (is_fp) return be_state->builder->CreateFCmp(pred, v1, v2);
             return be_state->builder->CreateICmp(pred, v1, v2);
           }
           break;
         case eok_cast:
         case eok_lvalue_cast: {
           if (v1) {
             llvm::Type* dest_ty = get_llvm_type(expr->type);
             llvm::Type* src_ty = v1->getType();
             if (src_ty == dest_ty) return v1;
             
             if (src_ty->isPointerTy() && dest_ty->isIntegerTy()) {
                return be_state->builder->CreatePtrToInt(v1, dest_ty);
             }
             if (src_ty->isIntegerTy() && dest_ty->isPointerTy()) {
                return be_state->builder->CreateIntToPtr(v1, dest_ty);
             }
             if (src_ty->isPointerTy() && dest_ty->isPointerTy()) {
                return v1;
             }
             
             bool src_is_fp = src_ty->isFloatingPointTy();
             bool dest_is_fp = dest_ty->isFloatingPointTy();
             
             if (src_is_fp && dest_is_fp) {
                if (src_ty->getPrimitiveSizeInBits() > dest_ty->getPrimitiveSizeInBits())
                   return be_state->builder->CreateFPTrunc(v1, dest_ty);
                return be_state->builder->CreateFPExt(v1, dest_ty);
             }
             
             if (src_is_fp && dest_ty->isIntegerTy()) {
                bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
                if (is_unsigned) return be_state->builder->CreateFPToUI(v1, dest_ty);
                return be_state->builder->CreateFPToSI(v1, dest_ty);
             }
             
             if (src_ty->isIntegerTy() && dest_is_fp) {
                bool src_is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
                if (src_is_unsigned) return be_state->builder->CreateUIToFP(v1, dest_ty);
                return be_state->builder->CreateSIToFP(v1, dest_ty);
             }
             
             if (src_ty->isIntegerTy() && dest_ty->isIntegerTy()) {
                unsigned src_bits = src_ty->getIntegerBitWidth();
                unsigned dest_bits = dest_ty->getIntegerBitWidth();
                if (dest_bits > src_bits) {
                   bool src_is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
                   if (src_is_unsigned) return be_state->builder->CreateZExt(v1, dest_ty);
                   return be_state->builder->CreateSExt(v1, dest_ty);
                }
                return be_state->builder->CreateTrunc(v1, dest_ty);
             }
             
             return be_state->builder->CreateBitCast(v1, dest_ty);
           }
           break;
         }
         case eok_assign:
           if (v1 && v2) {
             be_state->builder->CreateStore(v2, v1);
             return v2;
           }
           break;
         case enk_throw: {
           a_throw_supplement_ptr throw_info = expr->variant.throw_info;
           llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

           if (!throw_info) {
             // Rethrow
             llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("__cxa_rethrow",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
             if (!be_state->current_landing_pads.empty()) {
                 llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
                 llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
                 llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
                 be_state->builder->CreateInvoke(rethrow_fn, normal_bb, lpad_bb);
                 be_state->builder->SetInsertPoint(normal_bb);
             } else {
                 be_state->builder->CreateCall(rethrow_fn);
             }
             be_state->builder->CreateUnreachable();
             return nullptr;
           } else {
             // Allocate exception
             llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context); // Assume 64-bit size_t
             llvm::FunctionCallee alloc_fn = be_state->module->getOrInsertFunction("__cxa_allocate_exception",
                 llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false));
             
             uint32_t type_size = throw_info->type->size;
             if (type_size == 0) type_size = 1; // Itanium ABI enforces 1-byte minimum for empty classes
             llvm::Value* size_val = llvm::ConstantInt::get(size_t_ty, type_size);
             llvm::Value* exc_mem = be_state->builder->CreateCall(alloc_fn, {size_val});

             // Initialize exception object
             if ((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr)) {
               llvm::Value* init_val = emit_expression((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr));
               if (init_val) {
                 llvm::Type* exc_ty = get_llvm_type(throw_info->type);
                 if (exc_ty && exc_ty->isSized()) {
                   be_state->builder->CreateStore(init_val, exc_mem);
                 }
               }
             }

             // Throw exception
             llvm::FunctionCallee throw_fn = be_state->module->getOrInsertFunction("__cxa_throw",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty, int8_ptr_ty}, false));
             
             llvm::Value* typeinfo_ptr = get_typeinfo_global(throw_info->type);
             llvm::Value* dtor_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
             
             if (throw_info->dynamic_init && throw_info->dynamic_init->destructor) {
               a_routine_ptr dtor_rt = throw_info->dynamic_init->destructor;
               if (dtor_rt->source_corresp.name) {
                 llvm::Function* dtor_func = be_state->module->getFunction(dtor_rt->source_corresp.name);
                 if (!dtor_func) {
                   llvm::Type* func_ty = get_llvm_type(dtor_rt->type);
                   dtor_func = llvm::Function::Create(llvm::cast<llvm::FunctionType>(func_ty), llvm::GlobalValue::ExternalLinkage, dtor_rt->source_corresp.name, *be_state->module);
                 }
                 dtor_ptr = dtor_func;
               }
             }
             
             if (!be_state->current_landing_pads.empty()) {
                 llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
                 llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
                 llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
                 be_state->builder->CreateInvoke(throw_fn, normal_bb, lpad_bb, {exc_mem, typeinfo_ptr, dtor_ptr});
                 be_state->builder->SetInsertPoint(normal_bb);
             } else {
                 be_state->builder->CreateCall(throw_fn, {exc_mem, typeinfo_ptr, dtor_ptr});
             }
             be_state->builder->CreateUnreachable();
             return nullptr;
           }
         }
         case eok_call: {
           if (v1) {
             std::vector<llvm::Value*> args;
             for (an_expr_node_ptr arg = op1->next; arg != nullptr; arg = arg->next) {
               args.push_back(emit_expression(arg));
             }
             llvm::FunctionType* callee_ty = get_llvm_function_type(op1->type);
             if (!callee_ty) {
                if (llvm::Function* f = llvm::dyn_cast<llvm::Function>(v1)) {
                   callee_ty = f->getFunctionType();
                } else {
                   callee_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
                }
             }
             if (!be_state->current_landing_pads.empty()) {
               llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
               llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
               llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
               llvm::InvokeInst* invoke = be_state->builder->CreateInvoke(callee_ty, v1, normal_bb, lpad_bb, args);
               be_state->builder->SetInsertPoint(normal_bb);
               return invoke;
             } else {
               return be_state->builder->CreateCall(callee_ty, v1, args);
             }
           }
           break;
         }
         case eok_subscript: {
           if (v1 && v2) {
             llvm::Type* elem_ty = get_llvm_type(expr->type);
             llvm::Value* gep = be_state->builder->CreateInBoundsGEP(elem_ty, v1, v2);
             if (!expr->is_lvalue) {
               llvm::LoadInst* load = be_state->builder->CreateLoad(elem_ty, gep);
               if (is_volatile_qualified_type(expr->type)) {
                 load->setVolatile(true);
               }
               return load;
             }
             return gep;
           }
           break;
         }
         case eok_dot_field:
         case eok_points_to_field: {
           if (v1 && op2 && op2->kind == enk_field) {
             a_field_ptr field = op2->variant.field.ptr;
             if (field) {
               llvm::Value* offset_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), field->offset);
               llvm::Value* gep = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, offset_val);
               
               llvm::Type* field_llvm_ty = get_llvm_type(expr->type);
               
               if (!expr->is_lvalue) {
                 llvm::LoadInst* load = be_state->builder->CreateLoad(field_llvm_ty, gep);
                 if (is_volatile_qualified_type(expr->type)) {
                   load->setVolatile(true);
                 }
                 return load;
               }
               return gep;
             }
           }
           break;
         }
         case eok_indirect: {
           if (v1) {
             if (!expr->is_lvalue) {
               llvm::Type* load_ty = get_llvm_type(expr->type);
               llvm::LoadInst* load = be_state->builder->CreateLoad(load_ty, v1);
               if (is_volatile_qualified_type(expr->type)) {
                 load->setVolatile(true);
               }
               return load;
             }
             return v1; // already pointer
           }
           break;
         }
         case eok_address_of: {
           if (v1) {
             return v1; // v1 is already an lvalue pointer
           }
           break;
         }
         case eok_question: {
           an_expr_node_ptr cond_expr = op1;
           an_expr_node_ptr true_expr = op2;
           an_expr_node_ptr false_expr = op2 ? op2->next : nullptr;
           llvm::Value* cond = emit_expression(cond_expr);
           if (!cond->getType()->isIntegerTy(1)) {
               cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
           }
           
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "cond.true", func);
           llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "cond.false");
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "cond.end");
           
           be_state->builder->CreateCondBr(cond, true_bb, false_bb);
           
           be_state->builder->SetInsertPoint(true_bb);
           llvm::Value* true_val = emit_expression(true_expr);
           llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
           if (!true_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);
           
           func->insert(func->end(), false_bb);
           be_state->builder->SetInsertPoint(false_bb);
           llvm::Value* false_val = emit_expression(false_expr);
           llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
           if (!false_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);
           
           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);
           
           if (expr->type->kind != tk_void && true_val && false_val) {
             llvm::PHINode* phi = be_state->builder->CreatePHI(get_llvm_type(expr->type), 2, "cond.phi");
             phi->addIncoming(true_val, true_end_bb);
             phi->addIncoming(false_val, false_end_bb);
             return phi;
           }
           return nullptr;
         }
         case eok_land: {
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*be_state->context, "land.rhs", func);
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "land.end");

           llvm::Value* lhs_val = emit_expression(op1);
           llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(lhs_val, llvm::Constant::getNullValue(lhs_val->getType()));
           llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
           be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);

           be_state->builder->SetInsertPoint(rhs_bb);
           llvm::Value* rhs_val = emit_expression(op2);
           llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(rhs_val, llvm::Constant::getNullValue(rhs_val->getType()));
           llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
           if (!rhs_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);

           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);

           llvm::PHINode* phi = be_state->builder->CreatePHI(be_state->builder->getInt1Ty(), 2, "land.phi");
           phi->addIncoming(llvm::ConstantInt::getFalse(*be_state->context), lhs_end_bb);
           phi->addIncoming(rhs_cond, rhs_end_bb);
           return be_state->builder->CreateZExt(phi, get_llvm_type(expr->type));
         }
         case eok_lor: {
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*be_state->context, "lor.rhs", func);
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "lor.end");

           llvm::Value* lhs_val = emit_expression(op1);
           llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(lhs_val, llvm::Constant::getNullValue(lhs_val->getType()));
           llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
           be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);

           be_state->builder->SetInsertPoint(rhs_bb);
           llvm::Value* rhs_val = emit_expression(op2);
           llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(rhs_val, llvm::Constant::getNullValue(rhs_val->getType()));
           llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
           if (!rhs_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);

           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);

           llvm::PHINode* phi = be_state->builder->CreatePHI(be_state->builder->getInt1Ty(), 2, "lor.phi");
           phi->addIncoming(llvm::ConstantInt::getTrue(*be_state->context), lhs_end_bb);
           phi->addIncoming(rhs_cond, rhs_end_bb);
           return be_state->builder->CreateZExt(phi, get_llvm_type(expr->type));
         }
         default:
           // Stub for unsupported operations
           break;
       }
       return nullptr;
    }
    default:
       return nullptr;
  }
}

END_EDG_NAMESPACE
#endif
