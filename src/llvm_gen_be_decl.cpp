#include <type_traits>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "il_read.h"

#include <llvm/IR/Verifier.h>
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

void emit_global_variables() {
  if (!il_header.primary_scope) return;
  for (a_variable_ptr var = il_header.primary_scope->variables; var != nullptr; var = var->next) {
    if (!var->source_corresp.name) continue; // Skip unnamed
    
    llvm::Type* llvm_ty = get_llvm_type(var->type);
    
    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (var->storage_class == sc_static) {
      linkage = llvm::GlobalValue::InternalLinkage;
    }
    if (var->is_inline) {
      linkage = llvm::GlobalValue::LinkOnceODRLinkage;
    }
#if GNU_EXTENSIONS_ALLOWED
    if (var->is_weak || var->is_weakref) {
      linkage = llvm::GlobalValue::WeakAnyLinkage;
    }
#endif

    an_init_kind init_kind;
    an_initializer_ptr initializer_ptr;
    get_variable_initializer(var, il_header.primary_scope, &init_kind, &initializer_ptr);

    llvm::Constant* llvm_init = nullptr;
    if (init_kind == initk_static) {
       llvm_init = evaluate_constant(initializer_ptr->constant, llvm_ty);
    } else if (init_kind == initk_dynamic) {
       a_dynamic_init_ptr dip = initializer_ptr->dynamic;
       if (dip && dip->kind == dik_constant && !dip->follows_an_exec_statement) {
         llvm_init = llvm::Constant::getNullValue(llvm_ty);
       }
    }

    if (!llvm_init && var->storage_class != sc_extern) {
       // Default to zero-initialized for static/global without explicit init
       llvm_init = llvm::Constant::getNullValue(llvm_ty);
    }
    
    if (!llvm_init) {
       linkage = llvm::GlobalValue::ExternalLinkage;
    }

    be_state->module->getOrInsertGlobal(var->source_corresp.name, llvm_ty);
    llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(var->source_corresp.name);
    if (gvar) {
      gvar->setLinkage(linkage);
      if (var->is_thread_local) {
        gvar->setThreadLocalMode(llvm::GlobalValue::GeneralDynamicTLSModel);
      }
#if DO_IL_LOWERING
      if (var->comdat_group) {
        llvm::Comdat *comdat = be_state->module->getOrInsertComdat(var->comdat_group);
        gvar->setComdat(comdat);
      }
#endif
      if (llvm_init) {
        gvar->setInitializer(llvm_init);
      }
    }
  }
}

void emit_function_declarations() {
  if (!il_header.primary_scope) return;
  for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
    if (!routine->source_corresp.name) continue;
    
    llvm::Type* llvm_ty = get_llvm_type(routine->type);
    if (!llvm_ty->isFunctionTy()) continue;
    llvm::FunctionType* func_ty = llvm::cast<llvm::FunctionType>(llvm_ty);

    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (routine->function_def_number != 0) {
       if (routine->storage_class == sc_static) {
         linkage = llvm::GlobalValue::InternalLinkage;
       } else if (routine->is_inline) {
         linkage = llvm::GlobalValue::LinkOnceODRLinkage;
       }
    }

    llvm::Function* func = llvm::Function::Create(
        func_ty, linkage, routine->source_corresp.name, be_state->module.get());
        
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
    if (routine->always_inline) {
       func->addFnAttr(llvm::Attribute::AlwaysInline);
    }
    if (routine->never_inline) {
       func->addFnAttr(llvm::Attribute::NoInline);
    }
#endif
    if (routine->type && routine->type->kind == tk_routine && routine->type->variant.routine.extra_info && 
        routine->type->variant.routine.extra_info->does_not_return) {
       func->addFnAttr(llvm::Attribute::NoReturn);
    }

    if (routine->type && routine->type->kind == tk_routine && routine->type->variant.routine.extra_info) {
      unsigned param_idx = 0;
      for (a_param_type_ptr param = routine->type->variant.routine.extra_info->param_type_list;
           param != nullptr; param = param->next, ++param_idx) {
        if (param->type && (get_type_qualifiers(param->type) & TQ_RESTRICT) != 0) {
          func->addParamAttr(param_idx, llvm::Attribute::NoAlias);
        }
      }
    }
  }
}

static a_scope_ptr get_scope_for_routine_definition(a_routine_ptr rout) {
  a_memory_region_number region_number = mem_region_for_routine(rout);
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (!skip_il_read && mem_region_table[region_number] == NULL) {
    read_memory_region(region_number);
  }
#endif
  a_scope_ptr res = scope_for_routine(rout);
  llvm::errs() << "scope_for_routine for " << rout->source_corresp.name << " is " << (res ? "NOT NULL" : "NULL") << "\n";
  a_memory_region_number region_number2 = mem_region_for_routine(rout);
  llvm::errs() << "region_number is " << region_number2 << "\n";
  if (!res) {
     // fallback
     a_function_def_descr def_descr = il_header.function_def_table[rout->function_def_number];
     res = def_descr.scope;
  }
  return res;
}

void emit_function_definitions() {
  if (!il_header.primary_scope) return;

  for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
    if (!routine->source_corresp.name) continue;
    if (!routine->function_def_number) continue; // No body

    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) continue;

    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", func);
    be_state->builder->SetInsertPoint(entry_bb);

    // Look up the function definition
    a_scope_ptr func_scope = get_scope_for_routine_definition(routine);
    if (!func_scope) continue;

    // Clear local variables and labels for the new function scope
    be_state->local_vars.clear();
    be_state->label_blocks.clear();

    for (a_label_ptr label = func_scope->labels; label != nullptr; label = label->next) {
      be_state->label_blocks[label] = llvm::BasicBlock::Create(*be_state->context, "label", func);
    }

    // Allocate parameters and bind arguments
    unsigned arg_idx = 0;
    for (a_variable_ptr param = func_scope->variant.routine.parameters; param != nullptr; param = param->next, ++arg_idx) {
      if (arg_idx < func->arg_size()) {
        llvm::Argument* arg = func->getArg(arg_idx);
        if (param->source_corresp.name) {
          arg->setName(param->source_corresp.name);
        }

        llvm::Type* param_ty = get_llvm_type(param->type);
        llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(param_ty, nullptr, param->source_corresp.name ? std::string(param->source_corresp.name) + ".addr" : "");
        be_state->builder->CreateStore(arg, alloca);
        be_state->local_vars[param] = alloca;
      }
    }

    // Allocate local variables
    for (a_variable_ptr lvar = func_scope->nonstatic_variables; lvar != nullptr; lvar = lvar->next) {
      llvm::Type* lvar_ty = get_llvm_type(lvar->type);
      llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(lvar_ty, nullptr, lvar->source_corresp.name ? lvar->source_corresp.name : "");
      be_state->local_vars[lvar] = alloca;
    }

    // Map labels to BasicBlocks
    std::unordered_map<a_label_ptr, llvm::BasicBlock*> label_map;
    for (a_label_ptr lbl = func_scope->labels; lbl != nullptr; lbl = lbl->next) {
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, lbl->source_corresp.name ? lbl->source_corresp.name : "lbl", func);
      label_map[lbl] = bb;
    }

    if (func_scope->assoc_block) {
      llvm::errs() << "Emitting statements for " << routine->source_corresp.name << "\n";
      emit_statement(func_scope->assoc_block);
    }

    // Ensure all blocks have a terminator
    for (llvm::BasicBlock& bb : *func) {
      if (!bb.getTerminatorOrNull()) {
        llvm::errs() << "Adding terminator to " << bb.getName() << "\n";
        be_state->builder->SetInsertPoint(&bb);
        if (func->getReturnType()->isVoidTy()) {
          be_state->builder->CreateRetVoid();
        } else {
          be_state->builder->CreateUnreachable();
        }
      }
    }
    
    be_state->module->print(llvm::errs(), nullptr);
    llvm::errs() << "Verifying function " << routine->source_corresp.name << "\n";
    std::string err_str;
    llvm::raw_string_ostream os(err_str);
    if (llvm::verifyFunction(*func, &os)) {
      char error_msg[1024];
      snprintf(error_msg, sizeof(error_msg), "LLVM Function Verification failed for %s: %s", routine->source_corresp.name, err_str.c_str());
      
      // Map to source position
      
      internal_error(error_msg);
    }
  }
}

void emit_global_ctors_and_dtors() {
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
          llvm::Constant* fn_ptr = func;
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

END_EDG_NAMESPACE
#endif
