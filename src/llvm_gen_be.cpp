/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

llvm_gen_be.cpp - LLVM IR-generating back end

*/

#include "basics.h"

#if BACK_END_IS_LLVM_GEN_BE

#include "llvm_gen_be.h"
#include "host_envir.h"
#include "error.h"
#include "cfe.h"
#include "il.h"
#include "targ_def.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <string>

#include <unordered_map>

struct LLVMBackendState {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unordered_map<a_type_ptr, llvm::Type*> type_cache;
};

static LLVMBackendState* be_state = nullptr;

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

static llvm::Type* get_llvm_type(a_type_ptr edg_type) {
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
    case tk_integer:
    case tk_enum: {
      switch (edg_type->variant.integer.int_kind) {
        case ik_char:
        case ik_signed_char:
        case ik_unsigned_char:
          llvm_ty = llvm::IntegerType::get(*be_state->context, TARG_CHAR_BIT);
          break;
        case ik_short:
        case ik_unsigned_short:
          llvm_ty = llvm::IntegerType::get(*be_state->context, TARG_SHORT_BIT);
          break;
        case ik_int:
        case ik_unsigned_int:
          llvm_ty = llvm::IntegerType::get(*be_state->context, TARG_INT_BIT);
          break;
        case ik_long:
        case ik_unsigned_long:
          llvm_ty = llvm::IntegerType::get(*be_state->context, TARG_LONG_BIT);
          break;
#if LONG_LONG_ALLOWED
        case ik_long_long:
        case ik_unsigned_long_long:
          llvm_ty = llvm::IntegerType::get(*be_state->context, TARG_LONG_LONG_BIT);
          break;
#endif /* LONG_LONG_ALLOWED */
        default:
          llvm_ty = llvm::IntegerType::get(*be_state->context, edg_type->size * TARG_CHAR_BIT);
          break;
      }
      break;
    }
    case tk_float: {
      // Actually we should inspect the variant if available, but size is robust enough
      if (edg_type->size * TARG_CHAR_BIT == 32) {
        llvm_ty = llvm::Type::getFloatTy(*be_state->context);
      } else if (edg_type->size * TARG_CHAR_BIT == 64) {
        llvm_ty = llvm::Type::getDoubleTy(*be_state->context);
      } else if (edg_type->size * TARG_CHAR_BIT == 80) {
        llvm_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
      } else if (edg_type->size * TARG_CHAR_BIT == 128) {
        llvm_ty = llvm::Type::getFP128Ty(*be_state->context);
      } else if (edg_type->size * TARG_CHAR_BIT == 16) {
        llvm_ty = llvm::Type::getHalfTy(*be_state->context);
      } else {
        llvm_ty = llvm::Type::getDoubleTy(*be_state->context); // fallback
      }
      break;
    }
    case tk_pointer:
      // Opaque pointers in modern LLVM
      llvm_ty = llvm::PointerType::getUnqual(*be_state->context);
      break;
    case tk_array:
      llvm_ty = llvm::ArrayType::get(
          get_llvm_type(edg_type->variant.array.elem_type),
          edg_type->variant.array.elem_count);
      break;
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
    case tk_union:
    case tk_class: {
      // Forward declaration caching to avoid cycles
      llvm::StructType* struct_ty = llvm::StructType::create(*be_state->context);
      be_state->type_cache[edg_type] = struct_ty;
      
      std::vector<llvm::Type*> elem_tys;
      for (a_field_ptr field = edg_type->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
        elem_tys.push_back(get_llvm_type(field->type));
      }
      struct_ty->setBody(elem_tys, /*isPacked=*/false);
      llvm_ty = struct_ty;
      break;
    }
    case tk_enum:
      // Enums are lowered to their underlying integer type
      llvm_ty = get_llvm_type(edg_type->variant.enum_type.underlying_type);
      break;
    case tk_typeref:
      llvm_ty = get_llvm_type(edg_type->variant.typeref.type);
      break;
    default:
      // Fallback
      llvm_ty = llvm::Type::getInt8Ty(*be_state->context);
      break;
  }

  be_state->type_cache[edg_type] = llvm_ty;
  return llvm_ty;
}

static std::string build_data_layout() {
  std::string dl = "";
  // Endianness
#if TARG_LITTLE_ENDIAN
  dl += "e-";
#else
  dl += "E-";
#endif

  // Pointer size
  dl += "p:" + std::to_string(TARG_POINTER_SIZE * TARG_CHAR_BIT) + ":" + std::to_string(TARG_POINTER_SIZE * TARG_CHAR_BIT) + "-";

  // Data layout construction based on EDG target macros
  dl += "i8:" + std::to_string(TARG_CHAR_BIT) + "-";
  dl += "i16:" + std::to_string(TARG_SHORT_BIT) + "-";
  dl += "i32:" + std::to_string(TARG_INT_BIT) + "-";
  dl += "i64:" + std::to_string(TARG_LONG_LONG_BIT) + "-";
  dl += "f32:" + std::to_string(TARG_FLOAT_BIT) + "-";
  dl += "f64:" + std::to_string(TARG_DOUBLE_BIT) + "-";
  dl += "f128:" + std::to_string(TARG_LONG_DOUBLE_BIT);

  return dl;
}

static void emit_global_variables() {
  if (!il_header.primary_scope) return;
  for (a_variable_ptr var = il_header.primary_scope->variables; var != nullptr; var = var->next) {
    if (!var->source_corresp.name) continue; // Skip unnamed
    
    llvm::Type* llvm_ty = get_llvm_type(var->type);
    
    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (var->storage_class == sc_static) {
      linkage = llvm::GlobalValue::InternalLinkage;
    }

    an_init_kind init_kind;
    an_initializer_ptr initializer_ptr;
    get_variable_initializer(var, il_header.primary_scope, &init_kind, &initializer_ptr);

    llvm::Constant* llvm_init = nullptr;
    if (init_kind == initk_static) {
       // Placeholder for actual recursive constant translation
       llvm_init = llvm::Constant::getNullValue(llvm_ty);
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

    be_state->module->getOrInsertGlobal(var->source_corresp.name, llvm_ty);
    llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(var->source_corresp.name);
    if (gvar) {
      gvar->setLinkage(linkage);
      if (llvm_init) {
        gvar->setInitializer(llvm_init);
      }
    }
  }
}

static void emit_function_declarations() {
  if (!il_header.primary_scope) return;
  for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
    if (!routine->source_corresp.name) continue;
    
    llvm::Type* llvm_ty = get_llvm_type(routine->type);
    if (!llvm_ty->isFunctionTy()) continue;
    llvm::FunctionType* func_ty = llvm::cast<llvm::FunctionType>(llvm_ty);

    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (routine->storage_class == sc_static) {
      linkage = llvm::GlobalValue::InternalLinkage;
    }
    // Inline functions
    if (routine->is_inline) {
      linkage = llvm::GlobalValue::LinkOnceODRLinkage;
    }

    llvm::Function* func = llvm::Function::Create(
        func_ty, linkage, routine->source_corresp.name, be_state->module.get());
        
    // Example attributes based on EDG pragmas (simplified)
    // if (routine->source_corresp.some_noreturn_flag) func->addFnAttr(llvm::Attribute::NoReturn);
  }
}

static llvm::Value* emit_expression(an_expr_node_ptr expr) {
  if (!expr) return nullptr;

  switch (expr->kind) {
    case enk_constant: {
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) return nullptr;
       if (con->kind == ck_integer) {
         llvm::Type* ty = get_llvm_type(con->type);
         return llvm::ConstantInt::get(ty, con->variant.integer_constant.val, !con->variant.integer_constant.is_unsigned);
       } else if (con->kind == ck_float) {
         llvm::Type* ty = get_llvm_type(con->type);
         return llvm::ConstantFP::get(ty, con->variant.float_constant.val); // Simplified
       }
       return llvm::Constant::getNullValue(get_llvm_type(expr->type));
    }
    case enk_variable: {
       a_variable_ptr var = expr->variant.variable.ptr;
       // Assuming it's already allocated (either global or alloca)
       llvm::Value* ptr = be_state->module->getNamedGlobal(var->source_corresp.name ? var->source_corresp.name : "");
       if (!ptr) {
         // Fallback to searching local symbol table if we had one
         // For now just return null
       }
       // If lvalue, we return the pointer. If rvalue, we should load, but let's just return the pointer for now
       // and handle loads properly later. Actually, EDG handles lvalue-to-rvalue via operations or cast.
       return ptr;
    }
    case enk_operation: {
       an_expr_node_ptr op1 = expr->variant.operation.operands;
       an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
       llvm::Value* v1 = op1 ? emit_expression(op1) : nullptr;
       llvm::Value* v2 = op2 ? emit_expression(op2) : nullptr;
       
       switch (expr->variant.operation.operator_kind) {
         case eok_add:
           if (v1 && v2) return be_state->builder->CreateAdd(v1, v2);
           break;
         case eok_subtract:
           if (v1 && v2) return be_state->builder->CreateSub(v1, v2);
           break;
         case eok_multiply:
           if (v1 && v2) return be_state->builder->CreateMul(v1, v2);
           break;
         case eok_divide:
           if (v1 && v2) return be_state->builder->CreateSDiv(v1, v2); // Signed divide for simplicity
           break;
         case eok_eq:
           if (v1 && v2) return be_state->builder->CreateICmpEQ(v1, v2);
           break;
         case eok_assign:
           if (v1 && v2) {
             be_state->builder->CreateStore(v2, v1);
             return v2;
           }
           break;
         case eok_indirect:
           if (v1) {
             llvm::Type* load_ty = get_llvm_type(expr->type);
             return be_state->builder->CreateLoad(load_ty, v1);
           }
           break;
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

static void emit_statement(a_statement_ptr stmt);

static void emit_statement(a_statement_ptr stmt) {
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
    default:
      // Other statements (loops, switches) can be added here
      break;
  }
}

static void emit_function_definitions() {
  if (!il_header.primary_scope) return;

  for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
    if (!routine->source_corresp.name) continue;
    if (!routine->function_def_number) continue; // No body

    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) continue;

    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", func);
    be_state->builder->SetInsertPoint(entry_bb);

    // Look up the function definition
    a_function_def_descr def_descr = il_header.function_def_table[routine->function_def_number];
    a_scope_ptr func_scope = def_descr.scope;
    if (!func_scope) continue;

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
      }
    }

    // Allocate local variables
    for (a_variable_ptr lvar = func_scope->nonstatic_variables; lvar != nullptr; lvar = lvar->next) {
      llvm::Type* lvar_ty = get_llvm_type(lvar->type);
      be_state->builder->CreateAlloca(lvar_ty, nullptr, lvar->source_corresp.name ? lvar->source_corresp.name : "");
    }

    // Map labels to BasicBlocks
    std::unordered_map<a_label_ptr, llvm::BasicBlock*> label_map;
    for (a_label_ptr lbl = func_scope->labels; lbl != nullptr; lbl = lbl->next) {
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, lbl->name ? lbl->name : "lbl", func);
      label_map[lbl] = bb;
    }

    if (func_scope->assoc_block) {
      emit_statement(func_scope->assoc_block);
    }

    // Fallback terminator if body translation is not implemented yet
    if (!be_state->builder->GetInsertBlock()->getTerminator()) {
      if (func->getReturnType()->isVoidTy()) {
        be_state->builder->CreateRetVoid();
      } else {
        be_state->builder->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
      }
    }
  }
}

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

static void generate_llvm_output_file(const char* base_name) {
  std::string file_name;
  bool emit_bc = false; // Add CLI toggle logic later if needed

  if (gen_llvm_file_name) {
    file_name = gen_llvm_file_name;
  } else if (base_name) {
    const char* derived = derived_name(base_name, emit_bc ? ".bc" : ".ll");
    if (derived) file_name = derived;
  }

  if (file_name.empty()) {
    file_name = emit_bc ? "output.bc" : "output.ll";
  }

  std::string err_str;
  llvm::raw_string_ostream os(err_str);
  if (llvm::verifyModule(*be_state->module, &os)) {
    // If it fails, report via EDG error handling
    f_error(ec_generated_c, err_str.c_str());
  }

  std::error_code EC;
  llvm::raw_fd_ostream dest(file_name, EC, llvm::sys::fs::OF_None);

  if (EC) {
    f_error(ec_file_open_failed, file_name.c_str());
    return;
  }

  if (emit_bc) {
    llvm::WriteBitcodeToFile(*be_state->module, dest);
  } else {
    be_state->module->print(dest, nullptr);
  }
}

static void llvm_gen_be(void)
{
  be_state = new LLVMBackendState();
  be_state->context = std::make_unique<llvm::LLVMContext>();
  be_state->module = std::make_unique<llvm::Module>(
      primary_source_file_name ? primary_source_file_name : "edg_llvm_module",
      *be_state->context);
  be_state->builder = std::make_unique<llvm::IRBuilder<>>(*be_state->context);

  be_state->module->setDataLayout(build_data_layout());

  // Set a generic triple for now, or build it dynamically
  be_state->module->setTargetTriple(llvm::sys::getDefaultTargetTriple());

  emit_global_variables();
  emit_function_definitions();

  generate_llvm_output_file(primary_source_file_name);
}

#if !STANDALONE_UTILITY_PROGRAM

void back_end(void)
/*
Simple "back end" that generates LLVM IR. This version is for use as a
subroutine called in the same program as the front end.
*/
{
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  primary_source_file_name = NULL;
  if (skip_il_read) {
    /* The IL should still be in memory. */
  } else {
    il_read(f_il_output);
  }  /* if */
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Generate LLVM IR. */
  llvm_gen_be();
}

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE

void llvm_gen_be_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason. It performs any cleanup operations
required.
*/
{
  if (be_state) {
    delete be_state;
    be_state = nullptr;
  }
}

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */
