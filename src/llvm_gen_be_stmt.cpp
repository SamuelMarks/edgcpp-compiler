
/**
 * @file llvm_gen_be_stmt.cpp
 * @brief Statement lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end statement nodes (a_statement_ptr) into LLVM IR
 * control flow and instructions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_debug.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/InlineAsm.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Helper for lowering expr statement.
 * @param[in] stmt The EDG statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_expr_stmt(a_statement_ptr stmt) noexcept {
  llvm::Value* tmp_val = nullptr;
  return llvm_lower_expression(stmt->expr, &tmp_val);
}

llvm_gen_be_error_t llvm_lower_return_stmt(a_statement_ptr stmt) noexcept {
  if (stmt->expr) {
    llvm::Value* ret_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &ret_val);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::Type* func_ret_ty = func->getReturnType();

    if (func_ret_ty != ret_val->getType() && !func_ret_ty->isVoidTy()) {
      llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(ret_val->getType(), nullptr, "ret_pack");
      be_state->builder->CreateStore(ret_val, alloca);
      llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(alloca, llvm::PointerType::getUnqual(*be_state->context));
      ret_val = be_state->builder->CreateLoad(func_ret_ty, cast_ptr, "ret_packed");
    }

    be_state->builder->CreateRet(ret_val);
  } else {
    be_state->builder->CreateRetVoid();
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_if_stmt(a_statement_ptr stmt) noexcept {
  llvm::Value* cond = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &cond);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (cond && !cond->getType()->isIntegerTy(1)) {
    cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()), "cond");
  }
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();

  llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*be_state->context, "if.then", func);
  llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*be_state->context, "if.else");
  llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*be_state->context, "if.end");

  bool has_else = stmt->variant.if_stmt.else_statement != nullptr;
  be_state->builder->CreateCondBr(cond, then_bb, has_else ? else_bb : merge_bb);

  be_state->builder->SetInsertPoint(then_bb);
  err = llvm_lower_statement(stmt->variant.if_stmt.then_statement);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(merge_bb);
  }

  if (has_else) {
    func->insert(func->end(), else_bb);
    be_state->builder->SetInsertPoint(else_bb);
    err = llvm_lower_statement(stmt->variant.if_stmt.else_statement);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
      be_state->builder->CreateBr(merge_bb);
    }
  }

  func->insert(func->end(), merge_bb);
  be_state->builder->SetInsertPoint(merge_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_block_stmt(a_statement_ptr stmt) noexcept {
  llvm::DILexicalBlock* block = nullptr;
  if (be_state->dbg_state) {
    llvm_gen_be_error_t err = push_lexical_block(be_state->dbg_state, stmt->position, &block);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  for (a_statement_ptr s = stmt->variant.block.statements; s != nullptr; s = s->next) {
    llvm_gen_be_error_t err = llvm_lower_statement(s);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  if (be_state->dbg_state) {
    llvm_gen_be_error_t err = pop_lexical_block(be_state->dbg_state);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_while_stmt(a_statement_ptr stmt) noexcept {
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*be_state->context, "while.cond", func);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*be_state->context, "while.body");
  llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "while.end");

  be_state->builder->CreateBr(cond_bb);
  be_state->builder->SetInsertPoint(cond_bb);

  llvm::Value* cond = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &cond);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (cond && !cond->getType()->isIntegerTy(1)) {
    cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()), "cond");
  }
  be_state->builder->CreateCondBr(cond, body_bb, end_bb);

  func->insert(func->end(), body_bb);
  be_state->builder->SetInsertPoint(body_bb);

  be_state->break_blocks.push_back(end_bb);
  be_state->continue_blocks.push_back(cond_bb);

  err = llvm_lower_statement(stmt->variant.loop_statement);
  if (err != llvm_gen_be_error_t::ok) return err;

  be_state->break_blocks.pop_back();
  be_state->continue_blocks.pop_back();

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(cond_bb);
  }

  func->insert(func->end(), end_bb);
  be_state->builder->SetInsertPoint(end_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_for_stmt(a_statement_ptr stmt) noexcept {
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  
  if (stmt->variant.for_loop.extra_info->initialization) {
    llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.for_loop.extra_info->initialization);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*be_state->context, "for.cond", func);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*be_state->context, "for.body");
  llvm::BasicBlock* inc_bb = llvm::BasicBlock::Create(*be_state->context, "for.inc");
  llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "for.end");

  be_state->builder->CreateBr(cond_bb);
  be_state->builder->SetInsertPoint(cond_bb);

  if (stmt->expr) {
    llvm::Value* cond = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &cond);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (cond && !cond->getType()->isIntegerTy(1)) {
      cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()), "cond");
    }
    llvm::Value* cond_bool = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
    be_state->builder->CreateCondBr(cond_bool, body_bb, end_bb);
  } else {
    be_state->builder->CreateBr(body_bb);
  }

  func->insert(func->end(), body_bb);
  be_state->builder->SetInsertPoint(body_bb);

  be_state->break_blocks.push_back(end_bb);
  be_state->continue_blocks.push_back(inc_bb);

  llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.for_loop.statement);
  if (err != llvm_gen_be_error_t::ok) return err;

  be_state->break_blocks.pop_back();
  be_state->continue_blocks.pop_back();

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(inc_bb);
  }

  func->insert(func->end(), inc_bb);
  be_state->builder->SetInsertPoint(inc_bb);

  if (stmt->variant.for_loop.extra_info->increment) {
    llvm::Value* tmp_val = nullptr;
    err = llvm_lower_expression(stmt->variant.for_loop.extra_info->increment, &tmp_val);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  be_state->builder->CreateBr(cond_bb);

  func->insert(func->end(), end_bb);
  be_state->builder->SetInsertPoint(end_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_label_stmt(a_statement_ptr stmt) noexcept {
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  a_label_ptr label = stmt->variant.label.ptr;
  llvm::BasicBlock* label_bb = nullptr;
  if (be_state->label_blocks.count(label)) {
    label_bb = be_state->label_blocks[label];
    if (label_bb->getParent() == nullptr) {
       func->insert(func->end(), label_bb);
    } else {
       label_bb->moveAfter(be_state->builder->GetInsertBlock());
    }
  } else {
    label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
    be_state->label_blocks[label] = label_bb;
  }

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(label_bb);
  }
  be_state->builder->SetInsertPoint(label_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_goto_stmt(a_statement_ptr stmt) noexcept {
  a_label_ptr label = stmt->variant.label.ptr;
  llvm::BasicBlock* label_bb = nullptr;
  if (be_state->label_blocks.count(label)) {
    label_bb = be_state->label_blocks[label];
  } else {
    label_bb = llvm::BasicBlock::Create(*be_state->context, "label");
    be_state->label_blocks[label] = label_bb;
  }
  be_state->builder->CreateBr(label_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_assigned_goto_stmt(a_statement_ptr stmt) noexcept {
  llvm::Value* address = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &address);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  if (!address) {
    be_state->builder->CreateUnreachable();
    return llvm_gen_be_error_t::ok;
  }
  
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  unsigned num_dests = 0;
  for (auto& pair : be_state->label_blocks) {
    if (pair.first->address_taken) num_dests++;
  }

  if (num_dests > 0) {
    llvm::IndirectBrInst* indirect_br = be_state->builder->CreateIndirectBr(address, num_dests);
    for (auto& pair : be_state->label_blocks) {
      if (pair.first->address_taken) {
        llvm::BasicBlock* dest_bb = pair.second;
        if (dest_bb->getParent() == nullptr) {
          func->insert(func->end(), dest_bb);
        }
        indirect_br->addDestination(dest_bb);
      }
    }
  } else {
    be_state->builder->CreateUnreachable();
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_switch_stmt(a_statement_ptr stmt) noexcept {
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Value* cond = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &cond);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "switch.end");
  a_switch_stmt_descr_ptr descr = stmt->variant.switch_stmt.extra_info;
  llvm::BasicBlock* default_bb = end_bb;
  if (descr->default_case) {
    default_bb = llvm::BasicBlock::Create(*be_state->context, "sw.default");
    be_state->case_blocks[descr->default_case] = default_bb;
  }

  llvm::Type* cond_ty = cond->getType();
  unsigned num_cases = 0;
  for (a_switch_case_entry_ptr c = descr->cases; c != nullptr; c = c->next) {
    if (c != descr->default_case) {
       num_cases++;
       if (c->range_end) num_cases += 4;
    }
  }
  
  llvm::SwitchInst* switch_inst = be_state->builder->CreateSwitch(cond, default_bb, num_cases);
  
  bool is_signed = false;
  a_type_ptr expr_ty = stmt->expr ? stmt->expr->type : nullptr;
  if (expr_ty) {
      expr_ty = skip_typerefs(expr_ty);
      if (expr_ty->kind == tk_integer) {
          is_signed = int_kind_is_signed[expr_ty->variant.integer.int_kind];
      }
  }
  
  for (a_switch_case_entry_ptr c = descr->cases; c != nullptr; c = c->next) {
    if (c == descr->default_case) {
       if (default_bb->getParent() == nullptr) func->insert(func->end(), default_bb);
       continue;
    }
    llvm::BasicBlock* case_bb = llvm::BasicBlock::Create(*be_state->context, "sw.case", func);
    be_state->case_blocks[c] = case_bb;
    llvm::Constant* case_const = nullptr;
    err = evaluate_constant(c->case_value, cond_ty, &case_const);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::ConstantInt* start_val = llvm::cast<llvm::ConstantInt>(case_const);
    if (c->range_end) {
        llvm::Constant* range_const = nullptr;
        err = evaluate_constant(c->range_end, cond_ty, &range_const);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        llvm::ConstantInt* end_val = llvm::cast<llvm::ConstantInt>(range_const);
        llvm::APInt cur = start_val->getValue();
        llvm::APInt end = end_val->getValue();
        while (is_signed ? cur.sle(end) : cur.ule(end)) {
            switch_inst->addCase(llvm::ConstantInt::get(*be_state->context, cur), case_bb);
            if (cur == end) break;
            ++cur;
        }
    } else {
        switch_inst->addCase(start_val, case_bb);
    }
  }

  be_state->break_blocks.push_back(end_bb);
  err = llvm_lower_statement(stmt->variant.switch_stmt.body_statement);
  if (err != llvm_gen_be_error_t::ok) return err;
  be_state->break_blocks.pop_back();

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(end_bb);
  }

  func->insert(func->end(), end_bb);
  be_state->builder->SetInsertPoint(end_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_switch_case_stmt(a_statement_ptr stmt) noexcept {
  a_switch_case_entry_ptr c = stmt->variant.switch_case.extra_info;
  llvm::BasicBlock* case_bb = nullptr;
  if (be_state->case_blocks.count(c)) {
    case_bb = be_state->case_blocks[c];
  }
  if (case_bb) {
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
      be_state->builder->CreateBr(case_bb);
    }
    be_state->builder->SetInsertPoint(case_bb);
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_vla_stmt(a_statement_ptr stmt) noexcept {
  if (stmt->kind == stmk_set_vla_size) {
    a_vla_dimension_ptr dim = stmt->variant.vla_dimension;
    if (dim && dim->type) {
        be_state->array_to_vla_dim[dim->type] = dim;
    }
    if (dim && dim->dimension_variable && dim->dimension_expr) {
        llvm::Value* size_val = nullptr;
        llvm_gen_be_error_t err = llvm_lower_expression(dim->dimension_expr, &size_val);
        if (err != llvm_gen_be_error_t::ok) return err;
        if (be_state->local_vars.count(dim->dimension_variable)) {
            be_state->builder->CreateStore(size_val, be_state->local_vars[dim->dimension_variable]);
        }
    }
  } else if (stmt->kind == stmk_vla_decl) {
    if (!stmt->variant.vla.is_typedef_decl) {
        a_variable_ptr var = stmt->variant.vla.variant.variable;
        if (var && be_state->local_vars.count(var)) {
            llvm::Function* stacksave = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::stacksave, {llvm::PointerType::getUnqual(*be_state->context)});
            llvm::Value* saved_stack = be_state->builder->CreateCall(stacksave);
            be_state->vla_saved_stacks[var] = saved_stack;

            a_type_ptr array_ty = skip_typerefs(var->type);
            llvm::Value* total_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1);

            while (array_ty && array_ty->kind == tk_array) {
                llvm::Value* dim_size = nullptr;
                if (array_ty->variant.array.is_vla && array_ty->variant.array.has_assoc_vla_dimension) {
                    a_vla_dimension_ptr dim = nullptr;
                    if (be_state->array_to_vla_dim.count(array_ty)) {
                        dim = be_state->array_to_vla_dim[array_ty];
                    }
                    if (dim && dim->dimension_variable && be_state->local_vars.count(dim->dimension_variable)) {
                        llvm::AllocaInst* dim_alloca = llvm::cast<llvm::AllocaInst>(be_state->local_vars[dim->dimension_variable]);
                        llvm::Type* dim_ty = nullptr;
                        llvm_gen_be_error_t err_ty = get_llvm_type(dim->dimension_variable->type, &dim_ty);
                        if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                        dim_size = be_state->builder->CreateLoad(dim_ty, dim_alloca);
                        dim_size = be_state->builder->CreateZExtOrTrunc(dim_size, llvm::Type::getInt64Ty(*be_state->context));
                    }
                } else if (!array_ty->variant.array.bound_is_zero) {
                    dim_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), array_ty->variant.array.variant.number_of_elements);
                } else {
                    dim_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 0);
                }
                if (dim_size) {
                    total_size = be_state->builder->CreateMul(total_size, dim_size);
                }
                array_ty = skip_typerefs(array_ty->variant.array.element_type);
            }

            llvm::Type* elem_llvm_ty = nullptr;
            llvm_gen_be_error_t err_ty = get_llvm_type(array_ty, &elem_llvm_ty);
            if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
            llvm::AllocaInst* array_alloca = be_state->builder->CreateAlloca(elem_llvm_ty, total_size, var->source_corresp.name ? var->source_corresp.name : "vla");
            be_state->builder->CreateStore(array_alloca, be_state->local_vars[var]);
        }
    }
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept {
  an_asm_entry_ptr aep = stmt->variant.asm_entry;
  std::string asm_str;
  if (aep->asm_string && aep->asm_string->kind == ck_string) {
    asm_str = std::string(aep->asm_string->variant.string.value, aep->asm_string->variant.string.length - 1);
  }

  std::string constraints;
  std::vector<llvm::Value*> args;
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::Type*> arg_element_types;
  std::vector<llvm::Type*> output_types;
  std::vector<an_expr_node_ptr> output_exprs;

  bool first = true;
  for (an_asm_operand_ptr aop = aep->operands; aop != NULL; aop = aop->next) {
    if (!first) constraints += ",";
    first = false;

    bool output = aop->is_output_operand;
    std::string constr = aop->constraints_string ? aop->constraints_string : "";
    bool is_memory = (constr.find("m") != std::string::npos);
    
    if (is_memory) {
      if (output && constr.find("*") == std::string::npos) {
        if (constr.find("=") != std::string::npos) constr.insert(constr.find("=") + 1, "*");
        else if (constr.find("+") != std::string::npos) constr.replace(constr.find("+"), 1, "*");
      } else if (!output && constr.find("*") == std::string::npos) {
        if (constr.find("+") != std::string::npos) constr.replace(constr.find("+"), 1, "*");
        else constr = "*" + constr;
      }
    }
    constraints += constr;

    if (output && !is_memory) {
      llvm::Type* ty = nullptr;
      llvm_gen_be_error_t err_ty = get_llvm_type(aop->expression->type, &ty);
      if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
      output_types.push_back(ty);
      output_exprs.push_back(aop->expression);
    } else {
      llvm::Value* arg_val = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(aop->expression, &arg_val);
      if (err != llvm_gen_be_error_t::ok) return err;
      args.push_back(arg_val);
      arg_types.push_back(arg_val->getType());
      if (is_memory) {
         llvm::Type* mem_ty = nullptr;
         llvm_gen_be_error_t err_ty = get_llvm_type(aop->expression->type, &mem_ty);
         if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
         arg_element_types.push_back(mem_ty);
      } else {
         arg_element_types.push_back(nullptr);
      }
    }
  }

  for (a_named_register_list_ptr clob = aep->clobbers; clob != NULL; clob = clob->next) {
    if (!first) constraints += ",";
    first = false;
    constraints += "~{";
    constraints += named_register_names[(int)clob->reg];
    constraints += "}";
  }

  llvm::Type* ret_ty = nullptr;
  if (output_types.empty()) ret_ty = llvm::Type::getVoidTy(*be_state->context);
  else if (output_types.size() == 1) ret_ty = output_types[0];
  else ret_ty = llvm::StructType::get(*be_state->context, output_types);

  llvm::FunctionType* asm_func_ty = llvm::FunctionType::get(ret_ty, arg_types, false);
  llvm::InlineAsm* inline_asm = llvm::InlineAsm::get(asm_func_ty, asm_str, constraints, aep->is_volatile);
  llvm::CallInst* call = be_state->builder->CreateCall(inline_asm, args);
  for (unsigned i = 0; i < args.size(); ++i) {
    if (arg_element_types[i]) {
       call->addParamAttr(i, llvm::Attribute::get(*be_state->context, llvm::Attribute::ElementType, arg_element_types[i]));
    }
  }

  if (output_types.size() == 1) {
    llvm::Value* dst_ptr = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(output_exprs[0], &dst_ptr);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateStore(call, dst_ptr);
  } else if (output_types.size() > 1) {
    for (size_t i = 0; i < output_types.size(); ++i) {
      llvm::Value* ext = be_state->builder->CreateExtractValue(call, i);
      llvm::Value* dst_ptr = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(output_exprs[i], &dst_ptr);
      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateStore(ext, dst_ptr);
    }
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_try_block_stmt(a_statement_ptr stmt) noexcept {
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

  llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__gxx_personality_v0",
      llvm::FunctionType::get(int32_ty, true));
  func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

  llvm::BasicBlock* try_bb = llvm::BasicBlock::Create(*be_state->context, "try", func);
  llvm::BasicBlock* lpad_bb = llvm::BasicBlock::Create(*be_state->context, "lpad", func);
  llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "try.end");

  be_state->builder->CreateBr(try_bb);
  be_state->builder->SetInsertPoint(try_bb);

  be_state->current_landing_pads.push_back(lpad_bb);
  if (stmt->variant.try_block->statement) {
    llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.try_block->statement);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  be_state->current_landing_pads.pop_back();

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(end_try_bb);
  }

  be_state->builder->SetInsertPoint(lpad_bb);
  llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {int8_ptr_ty, int32_ty});
  llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 0);

  bool has_catch_all = false;
  for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
    if (!h->parameter) {
      has_catch_all = true;
      lpad->addClause(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)));
    } else {
      llvm::Constant* typeinfo_ptr = nullptr;
      llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
      if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
      lpad->addClause(typeinfo_ptr);
    }
  }
  lpad->setCleanup(true);

  llvm::Value* exc_ptr = be_state->builder->CreateExtractValue(lpad, 0, "exc_ptr");
  llvm::Value* exc_sel = be_state->builder->CreateExtractValue(lpad, 1, "exc_sel");

  llvm::FunctionCallee begin_catch_fn = be_state->module->getOrInsertFunction("__cxa_begin_catch",
     llvm::FunctionType::get(int8_ptr_ty, {int8_ptr_ty}, false));
  llvm::FunctionCallee end_catch_fn = be_state->module->getOrInsertFunction("__cxa_end_catch",
     llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
  llvm::FunctionCallee typeid_fn = be_state->module->getOrInsertFunction("llvm.eh.typeid.for",
     llvm::FunctionType::get(int32_ty, {int8_ptr_ty}, false));

  llvm::BasicBlock* resume_bb = llvm::BasicBlock::Create(*be_state->context, "resume", func);
  llvm::BasicBlock* current_dispatch_bb = be_state->builder->GetInsertBlock();

  for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
    llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "catch", func);
    llvm::BasicBlock* next_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "catch.fallthrough", func);

    be_state->builder->SetInsertPoint(current_dispatch_bb);
    
    if (!h->parameter) {
      be_state->builder->CreateBr(catch_bb);
    } else {
      llvm::Constant* typeinfo_ptr = nullptr;
      llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
      if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
      llvm::Value* typeid_val = be_state->builder->CreateCall(typeid_fn, {typeinfo_ptr});
      llvm::Value* cmp = be_state->builder->CreateICmpEQ(exc_sel, typeid_val);
      be_state->builder->CreateCondBr(cmp, catch_bb, next_dispatch_bb);
    }

    be_state->builder->SetInsertPoint(catch_bb);
    be_state->builder->CreateCall(begin_catch_fn, {exc_ptr});

    if (h->statement) {
      llvm_gen_be_error_t err = llvm_lower_statement(h->statement);
      if (err != llvm_gen_be_error_t::ok) return err;
    }

    be_state->builder->CreateCall(end_catch_fn);
    if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
      be_state->builder->CreateBr(end_try_bb);
    }

    current_dispatch_bb = next_dispatch_bb;
  }

  be_state->builder->SetInsertPoint(current_dispatch_bb);
  be_state->builder->CreateBr(resume_bb);

  be_state->builder->SetInsertPoint(resume_bb);
  be_state->builder->CreateResume(lpad);

  func->insert(func->end(), end_try_bb);
  be_state->builder->SetInsertPoint(end_try_bb);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
  if (!stmt) return llvm_gen_be_error_t::ok;

  if (be_state->dbg_state && !be_state->dbg_state->scope_stack.empty()) {
    llvm::DIScope* scope = be_state->dbg_state->scope_stack.back();
    llvm::DILocation* loc = nullptr;
    llvm_gen_be_error_t err = get_di_location(be_state->dbg_state, stmt->position, scope, &loc);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (loc) {
      be_state->builder->SetCurrentDebugLocation(loc);
    }
  }

  switch (stmt->kind) {
    case stmk_expr:
      return llvm_lower_expr_stmt(stmt);
    case stmk_return:
      return llvm_lower_return_stmt(stmt);
    case stmk_if:
      return llvm_lower_if_stmt(stmt);
    case stmk_block:
      return llvm_lower_block_stmt(stmt);
    case stmk_while:
      return llvm_lower_while_stmt(stmt);
    case stmk_for:
      return llvm_lower_for_stmt(stmt);
    case stmk_label:
      return llvm_lower_label_stmt(stmt);
    case stmk_goto:
      return llvm_lower_goto_stmt(stmt);
    case stmk_assigned_goto:
      return llvm_lower_assigned_goto_stmt(stmt);
    case stmk_switch:
      return llvm_lower_switch_stmt(stmt);
    case stmk_switch_case:
      return llvm_lower_switch_case_stmt(stmt);
    case stmk_set_vla_size:
    case stmk_vla_decl:
      return llvm_lower_vla_stmt(stmt);
    case stmk_asm:
      return llvm_lower_asm_stmt(stmt);
    case stmk_try_block:
      return llvm_lower_try_block_stmt(stmt);
    default:
      return llvm_gen_be_error_t::unsupported_stmt;
  }
}

END_EDG_NAMESPACE
#endif
