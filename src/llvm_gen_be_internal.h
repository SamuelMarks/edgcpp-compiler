#ifndef LLVM_GEN_BE_INTERNAL_H
#define LLVM_GEN_BE_INTERNAL_H

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be.h"
#include "llvm_gen_be_error.h"
#include "host_envir.h"
#include "error.h"
#include "il.h"
#include "targ_def.h"
#include "types.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_const.h"
#include "llvm_gen_be_expr.h"

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

struct llvm_gen_be_debug_state_t;

struct LLVMBackendState {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unordered_map<a_type_ptr, llvm::Type*> type_cache;
  std::unordered_map<a_variable_ptr, llvm::Value*> local_vars;
  std::unordered_map<a_variable_ptr, llvm::Value*> vla_saved_stacks;
  std::unordered_map<a_type_ptr, a_vla_dimension_ptr> array_to_vla_dim;

  std::vector<llvm::BasicBlock*> break_blocks;
  std::vector<llvm::BasicBlock*> continue_blocks;
  std::vector<llvm::BasicBlock*> current_landing_pads;
  std::unordered_map<a_label_ptr, llvm::BasicBlock*> label_blocks;
  std::unordered_map<a_switch_case_entry_ptr, llvm::BasicBlock*> case_blocks;
  
  llvm_gen_be_debug_state_t* dbg_state;
};

extern LLVMBackendState* be_state;

llvm::Type* get_llvm_type(a_type_ptr edg_type);
llvm::Constant* get_typeinfo_global(a_type_ptr type);

[[nodiscard]] llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt);

[[nodiscard]] llvm_gen_be_error_t llvm_lower_global_variables();
[[nodiscard]] llvm_gen_be_error_t llvm_lower_function_declarations();
[[nodiscard]] llvm_gen_be_error_t llvm_lower_function_definitions();
[[nodiscard]] llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors();
std::string build_data_layout();

END_EDG_NAMESPACE

#endif
