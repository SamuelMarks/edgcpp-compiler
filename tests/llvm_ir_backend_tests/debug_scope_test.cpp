#include <cassert>
#include <cstdio>
#include <memory>
#include "llvm_gen_be_debug.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

BEGIN_EDG_NAMESPACE

LLVMBackendState* be_state = nullptr;

// Mocks
a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
unsigned int targ_char_bit = 8;
a_targ_size_t targ_sizeof_short = 2;
a_targ_size_t targ_sizeof_int = 4;
a_targ_size_t targ_sizeof_long = 8;
a_targ_size_t targ_sizeof_long_long = 8;
a_targ_size_t targ_sizeof_pointer = 8;
a_source_file_ptr conv_seq_to_file_and_line(a_seq_number  seq_number,
                                            a_const_char  **file_name,
                                            a_const_char  **full_name,
                                            a_line_number *line_number,
                                            a_boolean     *at_end_of_source) {
  *file_name = "mock.cpp";
  *full_name = "/path/to/mock.cpp";
  *line_number = 10;
  *at_end_of_source = FALSE;
  return nullptr;
}

void run_tests() {
  printf("Running debug_scope_test...\n");

  LLVMBackendState state;
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_module", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  llvm_gen_be_debug_state_t* dbg_state = nullptr;
  llvm_gen_be_error_t err = debug_info_init(&dbg_state, be_state->module.get(), "test.cpp", ".", false);
  assert(err == llvm_gen_be_error_t::ok);
  be_state->dbg_state = dbg_state;

  // Pop invalid (empty stack)
  assert(pop_lexical_block(dbg_state) == llvm_gen_be_error_t::invalid_argument);

  // Push invalid (empty stack)
  a_source_position pos;
  pos.seq = 0;
  pos.column = 1;
  llvm::DILexicalBlock* block = nullptr;
  assert(push_lexical_block(dbg_state, pos, &block) == llvm_gen_be_error_t::invalid_argument);

  // Push onto stack with a subprogram
  a_routine dummy_routine;
  dummy_routine.source_corresp.name = "test_func";
  dummy_routine.source_corresp.decl_position.seq = 0;
  dummy_routine.storage_class = sc_extern;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
  llvm::Function* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", be_state->module.get());

  llvm::DISubprogram* subprog = nullptr;
  err = create_di_subprogram(dbg_state, &dummy_routine, fn, &subprog);
  assert(err == llvm_gen_be_error_t::ok);
  assert(subprog != nullptr);
  
  dbg_state->scope_stack.push_back(subprog);

  err = push_lexical_block(dbg_state, pos, &block);
  assert(err == llvm_gen_be_error_t::ok);
  assert(block != nullptr);
  assert(dbg_state->scope_stack.size() == 2);

  err = pop_lexical_block(dbg_state);
  assert(err == llvm_gen_be_error_t::ok);
  assert(dbg_state->scope_stack.size() == 1);

  // Instruction debug location
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", fn);
  be_state->builder->SetInsertPoint(bb);
  llvm::Instruction* ret = be_state->builder->CreateRetVoid();

  err = apply_instruction_debug_loc(ret, pos);
  assert(err == llvm_gen_be_error_t::ok);
  assert(ret->getDebugLoc());

  err = debug_info_finalize(dbg_state);
  assert(err == llvm_gen_be_error_t::ok);

  err = debug_info_cleanup(&dbg_state);
  assert(err == llvm_gen_be_error_t::ok);

  printf("All tests passed!\n");
}

END_EDG_NAMESPACE

int main() {
  edg::run_tests();
  return 0;
}