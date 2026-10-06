#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

void emit_statement(a_statement_ptr stmt) {
  if (!stmt) return;

  switch (stmt->kind) {
    case stmk_expr:
      emit_expression(stmt->expr);
      break;
    case stmk_return: {
      if (stmt->expr) {
        llvm::Value* ret_val = emit_expression(stmt->expr);
        be_state->builder->CreateRet(ret_val);
      } else {
        be_state->builder->CreateRetVoid();
      }
      break;
    }
    case stmk_if: {
      llvm::Value* cond = emit_expression(stmt->expr);
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*be_state->context, "if.then", func);
      llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*be_state->context, "if.else");
      llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*be_state->context, "if.end");

      bool has_else = stmt->variant.if_stmt.else_statement != nullptr;
      be_state->builder->CreateCondBr(cond, then_bb, has_else ? else_bb : merge_bb);

      be_state->builder->SetInsertPoint(then_bb);
      emit_statement(stmt->variant.if_stmt.then_statement);
      if (!be_state->builder->GetInsertBlock()->getTerminator()) {
        be_state->builder->CreateBr(merge_bb);
      }

      if (has_else) {
        func->insert(func->end(), else_bb);
        be_state->builder->SetInsertPoint(else_bb);
        emit_statement(stmt->variant.if_stmt.else_statement);
        if (!be_state->builder->GetInsertBlock()->getTerminator()) {
          be_state->builder->CreateBr(merge_bb);
        }
      }

      func->insert(func->end(), merge_bb);
      be_state->builder->SetInsertPoint(merge_bb);
      break;
    }
    case stmk_block: {
      for (a_statement_ptr s = stmt->variant.block.statements; s != nullptr; s = s->next) {
        emit_statement(s);
      }
      break;
    }
      case stmk_while: {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*be_state->context, "while.cond", func);
        llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*be_state->context, "while.body");
        llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "while.end");

        be_state->builder->CreateBr(cond_bb);
        be_state->builder->SetInsertPoint(cond_bb);

        llvm::Value* cond = emit_expression(stmt->expr);
        be_state->builder->CreateCondBr(cond, body_bb, end_bb);

        func->insert(func->end(), body_bb);
        be_state->builder->SetInsertPoint(body_bb);

        be_state->break_blocks.push_back(end_bb);
        be_state->continue_blocks.push_back(cond_bb);

        emit_statement(stmt->variant.loop_statement);

        be_state->break_blocks.pop_back();
        be_state->continue_blocks.pop_back();

        if (!be_state->builder->GetInsertBlock()->getTerminator()) {
          be_state->builder->CreateBr(cond_bb);
        }

        func->insert(func->end(), end_bb);
        be_state->builder->SetInsertPoint(end_bb);
        break;
      }
      case stmk_do: {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*be_state->context, "do.body", func);
        llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*be_state->context, "do.cond");
        llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "do.end");

        be_state->builder->CreateBr(body_bb);
        be_state->builder->SetInsertPoint(body_bb);

        be_state->break_blocks.push_back(end_bb);
        be_state->continue_blocks.push_back(cond_bb);

        emit_statement(stmt->variant.loop_statement);

        be_state->break_blocks.pop_back();
        be_state->continue_blocks.pop_back();

        if (!be_state->builder->GetInsertBlock()->getTerminator()) {
          be_state->builder->CreateBr(cond_bb);
        }

        func->insert(func->end(), cond_bb);
        be_state->builder->SetInsertPoint(cond_bb);

        llvm::Value* cond = emit_expression(stmt->expr);
        be_state->builder->CreateCondBr(cond, body_bb, end_bb);

        func->insert(func->end(), end_bb);
        be_state->builder->SetInsertPoint(end_bb);
        break;
      }
      case stmk_for: {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        
        if (stmt->variant.for_loop.extra_info->initialization) {
          emit_statement(stmt->variant.for_loop.extra_info->initialization);
        }

        llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*be_state->context, "for.cond", func);
        llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*be_state->context, "for.body");
        llvm::BasicBlock* inc_bb = llvm::BasicBlock::Create(*be_state->context, "for.inc");
        llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "for.end");

        be_state->builder->CreateBr(cond_bb);
        be_state->builder->SetInsertPoint(cond_bb);

        if (stmt->expr) {
          llvm::Value* cond = emit_expression(stmt->expr);
          be_state->builder->CreateCondBr(cond, body_bb, end_bb);
        } else {
          be_state->builder->CreateBr(body_bb);
        }

        func->insert(func->end(), body_bb);
        be_state->builder->SetInsertPoint(body_bb);

        be_state->break_blocks.push_back(end_bb);
        be_state->continue_blocks.push_back(inc_bb);

        emit_statement(stmt->variant.for_loop.statement);

        be_state->break_blocks.pop_back();
        be_state->continue_blocks.pop_back();

        if (!be_state->builder->GetInsertBlock()->getTerminator()) {
          be_state->builder->CreateBr(inc_bb);
        }

        func->insert(func->end(), inc_bb);
        be_state->builder->SetInsertPoint(inc_bb);

        if (stmt->variant.for_loop.extra_info->increment) {
          emit_expression(stmt->variant.for_loop.extra_info->increment);
        }
        be_state->builder->CreateBr(cond_bb);

        func->insert(func->end(), end_bb);
        be_state->builder->SetInsertPoint(end_bb);
        break;
static void emit_global_ctors_and_dtors() {
  std::vector<llvm::Constant*> ctors;
  std::vector<llvm::Constant*> dtors;
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* void_fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
  llvm::Type* ptr_ty = void_fn_ty->getPointerTo();
  llvm::StructType* ctor_struct_ty = llvm::StructType::get(
      int32_ty, ptr_ty, ptr_ty); // { i32, void ()*, i8* }

  if (il_header.primary_scope) {
    for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
      if (!routine->source_corresp.name) continue;
      
      bool is_ctor = routine->is_initialization_routine;
      bool is_dtor = routine->is_finalization_routine;
      int ctor_priority_val = 65535;
      int dtor_priority_val = 65535;

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (routine->has_ctor_priority) {
        is_ctor = true;
        ctor_priority_val = gnu_routine_supp(routine)->ctor_priority;
      }
      if (routine->has_dtor_priority) {
        is_dtor = true;
        dtor_priority_val = gnu_routine_supp(routine)->dtor_priority;
      }
#if DO_IL_LOWERING
      if (routine->is_initialization_routine && gnu_routine_supp_or_null(routine) && gnu_routine_supp(routine)->init_priority != 0) {
         ctor_priority_val = gnu_routine_supp(routine)->init_priority;
      }
      // Assuming finalization routines might also have init_priority if generated by lowering
      if (routine->is_finalization_routine && gnu_routine_supp_or_null(routine) && gnu_routine_supp(routine)->init_priority != 0) {
         dtor_priority_val = gnu_routine_supp(routine)->init_priority;
      }
#endif // DO_IL_LOWERING
#endif // GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
#endif // GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED

      if (is_ctor || is_dtor) {
        llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
        if (func) {
          llvm::Constant* fn_ptr = llvm::ConstantExpr::getBitCast(func, ptr_ty);
          llvm::Constant* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
          
          if (is_ctor) {
            llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, ctor_priority_val);
            ctors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, fn_ptr, null_ptr}));
          }
          if (is_dtor) {
            llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, dtor_priority_val);
            dtors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, fn_ptr, null_ptr}));
          }
        }
      }
    }
  }

  if (!ctors.empty()) {
    llvm::ArrayType* ctors_array_ty = llvm::ArrayType::get(ctor_struct_ty, ctors.size());
    llvm::Constant* ctors_array = llvm::ConstantArray::get(ctors_array_ty, ctors);
    new llvm::GlobalVariable(
        *be_state->module, ctors_array_ty, false,
        llvm::GlobalValue::AppendingLinkage, ctors_array, "llvm.global_ctors");
  }
  
  if (!dtors.empty()) {
    llvm::ArrayType* dtors_array_ty = llvm::ArrayType::get(ctor_struct_ty, dtors.size());
    llvm::Constant* dtors_array = llvm::ConstantArray::get(dtors_array_ty, dtors);
    new llvm::GlobalVariable(
        *be_state->module, dtors_array_ty, false,
        llvm::GlobalValue::AppendingLinkage, dtors_array, "llvm.global_dtors");
  }
}
      }
      case stmk_break: {
        if (!be_state->break_blocks.empty()) {
          be_state->builder->CreateBr(be_state->break_blocks.back());
        }
        break;
      }
      case stmk_continue: {
        if (!be_state->continue_blocks.empty()) {
          be_state->builder->CreateBr(be_state->continue_blocks.back());
        }
        break;
      }
      case stmk_label: {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        a_label_ptr label = stmt->variant.label.ptr;
        llvm::BasicBlock* label_bb = nullptr;
        if (be_state->label_blocks.count(label)) {
          label_bb = be_state->label_blocks[label];
          if (label_bb->getParent() == nullptr) {
             func->insert(func->end(), label_bb);
          }
        } else {
          label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
          be_state->label_blocks[label] = label_bb;
        }

        if (!be_state->builder->GetInsertBlock()->getTerminator()) {
          be_state->builder->CreateBr(label_bb);
        }
        be_state->builder->SetInsertPoint(label_bb);
        break;
      }
      case stmk_goto: {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        a_label_ptr label = stmt->variant.label.ptr;
        llvm::BasicBlock* label_bb = nullptr;
        if (be_state->label_blocks.count(label)) {
          label_bb = be_state->label_blocks[label];
        } else {
          label_bb = llvm::BasicBlock::Create(*be_state->context, "label");
          be_state->label_blocks[label] = label_bb;
        }
        be_state->builder->CreateBr(label_bb);
        break;
      }      case stmk_switch: {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        llvm::Value* cond = emit_expression(stmt->expr);
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
          if (c != descr->default_case) num_cases++;
        }
        
        llvm::SwitchInst* switch_inst = be_state->builder->CreateSwitch(cond, default_bb, num_cases);
        
        for (a_switch_case_entry_ptr c = descr->cases; c != nullptr; c = c->next) {
          if (c == descr->default_case) {
             if (default_bb->getParent() == nullptr) func->insert(func->end(), default_bb);
             continue;
          }
          llvm::BasicBlock* case_bb = llvm::BasicBlock::Create(*be_state->context, "sw.case", func);
          be_state->case_blocks[c] = case_bb;
          llvm::ConstantInt* val = llvm::cast<llvm::ConstantInt>(evaluate_constant(c->case_value, cond_ty));
          switch_inst->addCase(val, case_bb);
        }

        be_state->break_blocks.push_back(end_bb);
        
        emit_statement(stmt->variant.switch_stmt.body_statement);

        be_state->break_blocks.pop_back();

        if (!be_state->builder->GetInsertBlock()->getTerminator()) {
          be_state->builder->CreateBr(end_bb);
        }

        func->insert(func->end(), end_bb);
        be_state->builder->SetInsertPoint(end_bb);
        break;
      }
      case stmk_switch_case: {
        a_switch_case_entry_ptr c = stmt->variant.switch_case.extra_info;
        llvm::BasicBlock* case_bb = nullptr;
        if (be_state->case_blocks.count(c)) {
          case_bb = be_state->case_blocks[c];
        }
        if (case_bb) {
          if (!be_state->builder->GetInsertBlock()->getTerminator()) {
            be_state->builder->CreateBr(case_bb);
          }
          be_state->builder->SetInsertPoint(case_bb);
        }
        break;
      }    default:
      // Other statements (loops, switches) can be added here
      break;
    }
    }

END_EDG_NAMESPACE
#endif
