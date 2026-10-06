#ifndef LLVM_GEN_BE_INTERNAL_H
#define LLVM_GEN_BE_INTERNAL_H

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be.h"
#include "host_envir.h"
#include "error.h"
#include "il.h"
#include "targ_def.h"
#include "types.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

BEGIN_EDG_NAMESPACE

struct LLVMBackendState {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unordered_map<a_type_ptr, llvm::Type*> type_cache;
  std::unordered_map<a_variable_ptr, llvm::Value*> local_vars;

  std::vector<llvm::BasicBlock*> break_blocks;
  std::vector<llvm::BasicBlock*> continue_blocks;
  std::unordered_map<a_label_ptr, llvm::BasicBlock*> label_blocks;
  std::unordered_map<a_switch_case_entry_ptr, llvm::BasicBlock*> case_blocks;
};

extern LLVMBackendState* be_state;

llvm::Type* get_llvm_type(a_type_ptr edg_type);
llvm::Constant* evaluate_constant(a_constant_ptr con, llvm::Type* expected_ty);
llvm::Value* emit_expression(an_expr_node_ptr expr);
void emit_statement(a_statement_ptr stmt);

void emit_global_variables();
void emit_function_declarations();
void emit_function_definitions();
void emit_global_ctors_and_dtors();
std::string build_data_layout();

END_EDG_NAMESPACE

#endif
