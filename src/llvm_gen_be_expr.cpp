
/**
 * @file llvm_gen_be_expr.cpp
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_expr.h"
#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "target.h"
#include "lower_name.h"
#include <llvm/IR/Intrinsics.h>

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

[[nodiscard]] llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr) {
  if (!expr || !out_ptr) return llvm_gen_be_error_t::invalid_argument;
  
  switch (expr->kind) {
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
         return llvm_gen_be_error_t::unsupported_expr;
       }
       *out_ptr = ptr;
       return llvm_gen_be_error_t::ok;
    }
    case enk_operation: {
       an_expr_node_ptr op1 = expr->variant.operation.operands;
       an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
       
       switch (expr->variant.operation.kind) {
         case eok_subscript: {
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           err = llvm_lower_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           llvm::Type* elem_ty = get_llvm_type(expr->type);
           *out_ptr = be_state->builder->CreateInBoundsGEP(elem_ty, v1, v2);
           return llvm_gen_be_error_t::ok;
         }
         case eok_dot_field:
         case eok_points_to_field: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (v1 && op2 && op2->kind == enk_field) {
             a_field_ptr field = op2->variant.field.ptr;
             if (field) {
               llvm::Value* offset_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), field->offset);
               *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, offset_val);
               return llvm_gen_be_error_t::ok;
             }
           }
           return llvm_gen_be_error_t::unsupported_expr;
         }
         case eok_indirect: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_ptr = v1;
           return llvm_gen_be_error_t::ok;
         }
         default:
           return llvm_gen_be_error_t::unsupported_expr;
       }
    }
    default:
       return llvm_gen_be_error_t::unsupported_expr;
  }
}

[[nodiscard]] llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
  
  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
  if (op1) { err = llvm_lower_expression(op1, &v1); if (err != llvm_gen_be_error_t::ok) return err; }
  if (op2) { err = llvm_lower_expression(op2, &v2); if (err != llvm_gen_be_error_t::ok) return err; }
  
  if (!v1 || !v2) return llvm_gen_be_error_t::unsupported_expr;
  
  switch (expr->variant.operation.kind) {
    case eok_add:
      if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFAdd(v1, v2);
      else *out_val = be_state->builder->CreateAdd(v1, v2);
      break;
    case eok_subtract:
      if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFSub(v1, v2);
      else *out_val = be_state->builder->CreateSub(v1, v2);
      break;
    case eok_multiply:
      if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFMul(v1, v2);
      else *out_val = be_state->builder->CreateMul(v1, v2);
      break;
    case eok_divide: {
      if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFDiv(v1, v2);
      else {
        bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
        if (is_unsigned) *out_val = be_state->builder->CreateUDiv(v1, v2);
        else *out_val = be_state->builder->CreateSDiv(v1, v2);
      }
      break;
    }
    case eok_remainder: {
      if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFRem(v1, v2);
      else {
        bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
        if (is_unsigned) *out_val = be_state->builder->CreateURem(v1, v2);
        else *out_val = be_state->builder->CreateSRem(v1, v2);
      }
      break;
    }
    case eok_and: *out_val = be_state->builder->CreateAnd(v1, v2); break;
    case eok_or: *out_val = be_state->builder->CreateOr(v1, v2); break;
    case eok_xor: *out_val = be_state->builder->CreateXor(v1, v2); break;
    case eok_shiftl: *out_val = be_state->builder->CreateShl(v1, v2); break;
    case eok_shiftr: {
      bool is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
      if (is_unsigned) *out_val = be_state->builder->CreateLShr(v1, v2);
      else *out_val = be_state->builder->CreateAShr(v1, v2);
      break;
    }
    default: return llvm_gen_be_error_t::unsupported_expr;
  }
  return llvm_gen_be_error_t::ok;
}

[[nodiscard]] llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
  
  if (expr->variant.operation.kind == eok_land || expr->variant.operation.kind == eok_lor) {
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.rhs" : "lor.rhs", func);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.end" : "lor.end");

    llvm::Value* lhs_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(op1, &lhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(lhs_val, llvm::Constant::getNullValue(lhs_val->getType()));
    llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
    
    if (expr->variant.operation.kind == eok_land) be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);
    else be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);

    be_state->builder->SetInsertPoint(rhs_bb);
    llvm::Value* rhs_val = nullptr;
    err = llvm_lower_expression(op2, &rhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(rhs_val, llvm::Constant::getNullValue(rhs_val->getType()));
    llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
    if (!rhs_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);

    func->insert(func->end(), end_bb);
    be_state->builder->SetInsertPoint(end_bb);

    llvm::PHINode* phi = be_state->builder->CreatePHI(be_state->builder->getInt1Ty(), 2, 
      (expr->variant.operation.kind == eok_land) ? "land.phi" : "lor.phi");
    phi->addIncoming((expr->variant.operation.kind == eok_land) ? llvm::ConstantInt::getFalse(*be_state->context) : llvm::ConstantInt::getTrue(*be_state->context), lhs_end_bb);
    phi->addIncoming(rhs_cond, rhs_end_bb);
    *out_val = be_state->builder->CreateZExt(phi, get_llvm_type(expr->type));
    return llvm_gen_be_error_t::ok;
  }
  
  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
  if (op1) { err = llvm_lower_expression(op1, &v1); if (err != llvm_gen_be_error_t::ok) return err; }
  if (op2) { err = llvm_lower_expression(op2, &v2); if (err != llvm_gen_be_error_t::ok) return err; }
  if (!v1 || !v2) return llvm_gen_be_error_t::unsupported_expr;
  
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
     default: return llvm_gen_be_error_t::unsupported_expr;
  }
  if (is_fp) *out_val = be_state->builder->CreateFCmp(pred, v1, v2);
  else *out_val = be_state->builder->CreateICmp(pred, v1, v2);
  return llvm_gen_be_error_t::ok;
}

[[nodiscard]] llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  if (!v1) return llvm_gen_be_error_t::unsupported_expr;
  
  llvm::Type* dest_ty = get_llvm_type(expr->type);
  llvm::Type* src_ty = v1->getType();
  if (src_ty == dest_ty) {
      *out_val = v1;
      return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isPointerTy() && dest_ty->isIntegerTy()) {
     *out_val = be_state->builder->CreatePtrToInt(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  if (src_ty->isIntegerTy() && dest_ty->isPointerTy()) {
     *out_val = be_state->builder->CreateIntToPtr(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  if (src_ty->isPointerTy() && dest_ty->isPointerTy()) {
     *out_val = v1;
     return llvm_gen_be_error_t::ok;
  }
  
  bool src_is_fp = src_ty->isFloatingPointTy();
  bool dest_is_fp = dest_ty->isFloatingPointTy();
  
  if (src_is_fp && dest_is_fp) {
     if (src_ty->getPrimitiveSizeInBits() > dest_ty->getPrimitiveSizeInBits())
        *out_val = be_state->builder->CreateFPTrunc(v1, dest_ty);
     else
        *out_val = be_state->builder->CreateFPExt(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_is_fp && dest_ty->isIntegerTy()) {
     bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
     if (is_unsigned) *out_val = be_state->builder->CreateFPToUI(v1, dest_ty);
     else *out_val = be_state->builder->CreateFPToSI(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isIntegerTy() && dest_is_fp) {
     bool src_is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
     if (src_is_unsigned) *out_val = be_state->builder->CreateUIToFP(v1, dest_ty);
     else *out_val = be_state->builder->CreateSIToFP(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isIntegerTy() && dest_ty->isIntegerTy()) {
     unsigned src_bits = src_ty->getIntegerBitWidth();
     unsigned dest_bits = dest_ty->getIntegerBitWidth();
     if (dest_bits > src_bits) {
        bool src_is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
        if (src_is_unsigned) *out_val = be_state->builder->CreateZExt(v1, dest_ty);
        else *out_val = be_state->builder->CreateSExt(v1, dest_ty);
     } else {
        *out_val = be_state->builder->CreateTrunc(v1, dest_ty);
     }
     return llvm_gen_be_error_t::ok;
  }
  
  *out_val = be_state->builder->CreateBitCast(v1, dest_ty);
  return llvm_gen_be_error_t::ok;
}

[[nodiscard]] llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (!v1) return llvm_gen_be_error_t::unsupported_expr;
  
  llvm::FunctionType* callee_ty = nullptr;
  llvm::AttributeList callee_attrs;
  if (op1->type) {
     a_type_ptr func_ty_edg = op1->type;
     if (func_ty_edg->kind == tk_pointer) func_ty_edg = func_ty_edg->variant.array.element_type;
     if (func_ty_edg->kind == tk_routine) {
         llvm::Triple triple(be_state->module->getTargetTriple());
         if (triple.isAArch64()) {
           err = build_aapcs64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         } else {
           err = build_sysv_function_type(func_ty_edg, &callee_ty, &callee_attrs);
         }
         if (err != llvm_gen_be_error_t::ok) return err;
     }
  }

  std::vector<llvm::Value*> args;
  unsigned callee_arg_idx = 0;
  
  if (callee_ty && callee_ty->getNumParams() > 0) {
      // Check for sret
      if (callee_attrs.hasParamAttr(0, llvm::Attribute::StructRet)) {
          // The first parameter is sret, allocate it and pass as arg
          llvm::Type* sret_ty = callee_attrs.getParamStructRetType(0);
          llvm::AllocaInst* sret_alloc = be_state->builder->CreateAlloca(sret_ty, nullptr, "sret");
          args.push_back(sret_alloc);
          callee_arg_idx++;
      }
  }

  for (an_expr_node_ptr arg = op1->next; arg != nullptr; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    if (callee_ty && callee_arg_idx < callee_ty->getNumParams()) {
        llvm::Type* expected_ty = callee_ty->getParamType(callee_arg_idx);
        if (callee_attrs.hasParamAttr(callee_arg_idx, llvm::Attribute::ByVal)) {
            // Needs a pointer for byval
            if (!arg_val->getType()->isPointerTy()) {
                llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
                be_state->builder->CreateStore(arg_val, alloc);
                arg_val = alloc;
            }
            if (arg_val->getType() != expected_ty) {
                arg_val = be_state->builder->CreatePointerCast(arg_val, expected_ty);
            }
        } else if (arg_val->getType() != expected_ty) {
            // Direct register packed struct coercion
            llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
            be_state->builder->CreateStore(arg_val, alloc);
            llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(alloc, llvm::PointerType::getUnqual(*be_state->context));
            arg_val = be_state->builder->CreateLoad(expected_ty, cast_ptr);
        }
    }
    
    args.push_back(arg_val);
    callee_arg_idx++;
  }
  
  if (!callee_ty) {
     callee_ty = get_llvm_function_type(op1->type);
  }

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
    *out_val = invoke;
  } else {
    *out_val = be_state->builder->CreateCall(callee_ty, v1, args);
  }
  return llvm_gen_be_error_t::ok;
}

[[nodiscard]] llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  if (!expr) {
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  // Handle LValue evaluation explicitly
  if (expr->is_lvalue) {
    switch (expr->kind) {
      case enk_variable:
      case enk_operation:
        if (expr->kind == enk_operation) {
           an_expr_operator_kind op_kind = expr->variant.operation.kind;
           if (op_kind != eok_subscript && op_kind != eok_dot_field && 
               op_kind != eok_points_to_field && op_kind != eok_indirect) {
               break; // Not an LValue-producing operation
           }
        }
        return llvm_lower_lvalue_expression(expr, out_val);
      default: break;
    }
  }

  switch (expr->kind) {
    case enk_constant: {
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) {
           *out_val = llvm::Constant::getNullValue(get_llvm_type(expr->type));
           return llvm_gen_be_error_t::ok;
       }
       llvm::Constant* out_const = nullptr;
       llvm_gen_be_error_t err = evaluate_constant(con, get_llvm_type(expr->type), &out_const);
       if (err != llvm_gen_be_error_t::ok) return err;
       *out_val = out_const;
       return llvm_gen_be_error_t::ok;
    }
    case enk_routine: {
       a_routine_ptr routine = expr->variant.routine.ptr;
       if (!routine || !routine->source_corresp.name) {
           *out_val = llvm::Constant::getNullValue(get_llvm_type(expr->type));
           return llvm_gen_be_error_t::ok;
       }
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
       *out_val = func;
       return llvm_gen_be_error_t::ok;
    }
    case enk_variable: {
       llvm::Value* ptr = nullptr;
       llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
       if (err != llvm_gen_be_error_t::ok) return err;
       
       llvm::LoadInst* load = be_state->builder->CreateLoad(get_llvm_type(expr->type), ptr);
       if (is_volatile_qualified_type(expr->type)) load->setVolatile(true);
       *out_val = load;
       return llvm_gen_be_error_t::ok;
    }
    case enk_operation: {
       switch (expr->variant.operation.kind) {
         case eok_add:
         case eok_subtract:
         case eok_multiply:
         case eok_divide:
         case eok_remainder:
         case eok_and:
         case eok_or:
         case eok_xor:
         case eok_shiftl:
         case eok_shiftr:
           return llvm_lower_arithmetic_expression(expr, out_val);
           
         case eok_eq:
         case eok_ne:
         case eok_lt:
         case eok_le:
         case eok_gt:
         case eok_ge:
         case eok_land:
         case eok_lor:
           return llvm_lower_logical_expression(expr, out_val);
           
         case eok_cast:
         case eok_lvalue_cast:
           return llvm_lower_cast_expression(expr, out_val);
           
         case eok_call:
           return llvm_lower_call_expression(expr, out_val);
           
         case eok_subscript:
         case eok_dot_field:
         case eok_points_to_field:
         case eok_indirect: {
           llvm::Value* ptr = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (expr->variant.operation.kind == eok_dot_field || expr->variant.operation.kind == eok_points_to_field) {
             an_expr_node_ptr op2 = expr->variant.operation.operands->next;
             if (op2 && op2->kind == enk_field) {
                 a_field_ptr field = op2->variant.field.ptr;
                 if (field && field->is_bit_field) {
                   llvm::Type* field_llvm_ty = get_llvm_type(expr->type);
                   llvm::LoadInst* load = be_state->builder->CreateLoad(field_llvm_ty, ptr);
                   if (is_volatile_qualified_type(expr->type)) load->setVolatile(true);
                   
                   unsigned bit_offset = field->offset_bit_remainder;
                   unsigned bit_size = field->bit_size;
                   unsigned container_bits = field_llvm_ty->getIntegerBitWidth();
                   unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
                   
                   llvm::Value* val = load;
                   if (shift > 0) val = be_state->builder->CreateLShr(val, shift);
                   
                   uint64_t mask = (1ULL << bit_size) - 1;
                   val = be_state->builder->CreateAnd(val, mask);
                   
                   if (field->bit_field_is_signed) {
                     unsigned shift_up = container_bits - bit_size;
                     if (shift_up > 0) {
                       val = be_state->builder->CreateShl(val, shift_up);
                       val = be_state->builder->CreateAShr(val, shift_up);
                     }
                   }
                   *out_val = val;
                   return llvm_gen_be_error_t::ok;
                 }
             }
           }
           
           llvm::LoadInst* load = be_state->builder->CreateLoad(get_llvm_type(expr->type), ptr);
           if (is_volatile_qualified_type(expr->type)) load->setVolatile(true);
           *out_val = load;
           return llvm_gen_be_error_t::ok;
         }
         case eok_address_of: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = v1;
           return llvm_gen_be_error_t::ok;
         }
         case eok_question: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr cond_expr = op1;
           an_expr_node_ptr true_expr = op1 ? op1->next : nullptr;
           an_expr_node_ptr false_expr = true_expr ? true_expr->next : nullptr;
           
           llvm::Value* cond = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(cond_expr, &cond);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (!cond->getType()->isIntegerTy(1)) {
               cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
           }
           
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "cond.true", func);
           llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "cond.false");
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "cond.end");
           
           be_state->builder->CreateCondBr(cond, true_bb, false_bb);
           
           be_state->builder->SetInsertPoint(true_bb);
           llvm::Value* true_val = nullptr;
           err = llvm_lower_expression(true_expr, &true_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
           if (!true_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);
           
           func->insert(func->end(), false_bb);
           be_state->builder->SetInsertPoint(false_bb);
           llvm::Value* false_val = nullptr;
           err = llvm_lower_expression(false_expr, &false_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
           if (!false_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);
           
           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);
           
           if (expr->type->kind != tk_void && true_val && false_val) {
             llvm::PHINode* phi = be_state->builder->CreatePHI(get_llvm_type(expr->type), 2, "cond.phi");
             phi->addIncoming(true_val, true_end_bb);
             phi->addIncoming(false_val, false_end_bb);
             *out_val = phi;
           } else {
             *out_val = nullptr;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_assign: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           err = llvm_lower_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (v1 && v2) {
             bool is_bitfield_assign = false;
             if (op1 && op1->kind == enk_operation && 
                 (op1->variant.operation.kind == eok_dot_field || op1->variant.operation.kind == eok_points_to_field)) {
                 an_expr_node_ptr sub_op2 = op1->variant.operation.operands->next;
                 if (sub_op2 && sub_op2->kind == enk_field) {
                     a_field_ptr field = sub_op2->variant.field.ptr;
                     if (field && field->is_bit_field) {
                         is_bitfield_assign = true;
                         llvm::Type* container_ty = get_llvm_type(op1->type);
                         
                         llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, v1);
                         if (is_volatile_qualified_type(op1->type)) load->setVolatile(true);
                         
                         unsigned bit_offset = field->offset_bit_remainder;
                         unsigned bit_size = field->bit_size;
                         unsigned container_bits = container_ty->getIntegerBitWidth();
                         unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
                         
                         llvm::Value* val = v2;
                         if (val->getType()->getIntegerBitWidth() > container_bits) {
                             val = be_state->builder->CreateTrunc(val, container_ty);
                         } else if (val->getType()->getIntegerBitWidth() < container_bits) {
                             val = be_state->builder->CreateZExt(val, container_ty);
                         }
                         
                         uint64_t val_mask = (1ULL << bit_size) - 1;
                         val = be_state->builder->CreateAnd(val, val_mask);
                         
                         llvm::Value* shifted_val = val;
                         if (shift > 0) shifted_val = be_state->builder->CreateShl(val, shift);
                         
                         uint64_t container_mask = ~(val_mask << shift);
                         llvm::Value* masked_container = be_state->builder->CreateAnd(load, llvm::ConstantInt::get(container_ty, container_mask));
                         
                         llvm::Value* new_container = be_state->builder->CreateOr(masked_container, shifted_val);
                         
                         llvm::StoreInst* store = be_state->builder->CreateStore(new_container, v1);
                         if (is_volatile_qualified_type(op1->type)) store->setVolatile(true);
                     }
                 }
             }
             
             if (!is_bitfield_assign) {
                 llvm::Type* ty = get_llvm_type(expr->type);
                 if (ty->isAggregateType() && be_state->module->getDataLayout().getTypeStoreSize(ty) > 16) {
                     if (llvm::LoadInst* li = llvm::dyn_cast<llvm::LoadInst>(v2)) {
                         llvm::Value* src_ptr = li->getPointerOperand();
                         uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(ty);
                         llvm::Align align = be_state->module->getDataLayout().getABITypeAlign(ty);
                         be_state->builder->CreateMemCpy(v1, align, src_ptr, align, size);
                     } else {
                         be_state->builder->CreateStore(v2, v1);
                     }
                 } else {
                     be_state->builder->CreateStore(v2, v1);
                 }
             }
             *out_val = v2;
             return llvm_gen_be_error_t::ok;
           }
           return llvm_gen_be_error_t::unsupported_expr;
         }
         default:
           return llvm_gen_be_error_t::unsupported_expr;
       }
    }
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
         *out_val = nullptr;
         return llvm_gen_be_error_t::ok;
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
           llvm::Value* init_val = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr), &init_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           
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
         *out_val = nullptr;
         return llvm_gen_be_error_t::ok;
       }
    }
    case enk_vla_dealloc: {
#if VLA_DEALLOCATIONS_IN_IL
      a_variable_ptr var = expr->variant.vla_variable;
      if (be_state->vla_saved_stacks.count(var)) {
          llvm::Function* stackrestore = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::stackrestore, {llvm::PointerType::getUnqual(*be_state->context)});
          be_state->builder->CreateCall(stackrestore, be_state->vla_saved_stacks[var]);
      }
#endif
      *out_val = nullptr;
      return llvm_gen_be_error_t::ok;
    }
    default:
       return llvm_gen_be_error_t::unsupported_expr;
  }
}

END_EDG_NAMESPACE
#endif
