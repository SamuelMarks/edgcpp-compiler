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
a_byte_boolean int_kind_is_signed[ik_last] = {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE};
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
  printf("Running debug_types_test...\n");

  LLVMBackendState state;
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_module", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  llvm_gen_be_debug_state_t* dbg_state = nullptr;
  llvm_gen_be_error_t err = debug_info_init(&dbg_state, be_state->module.get(), "test.cpp", ".", false);
  assert(err == llvm_gen_be_error_t::ok);
  be_state->dbg_state = dbg_state;

  // Primitive Type
  {
    a_type dummy_int = {};
    dummy_int.kind = tk_integer;
    dummy_int.size = 4;
    dummy_int.variant.integer.int_kind = ik_int;

    llvm::DIType* di_ty = nullptr;
    err = get_or_create_di_type(dbg_state, &dummy_int, &di_ty);
    assert(err == llvm_gen_be_error_t::ok);
    assert(di_ty != nullptr);

    // Test caching
    llvm::DIType* di_ty2 = nullptr;
    err = get_or_create_di_type(dbg_state, &dummy_int, &di_ty2);
    assert(err == llvm_gen_be_error_t::ok);
    assert(di_ty == di_ty2);
  }

  // Pointer Type
  {
    a_type dummy_int = {};
    dummy_int.kind = tk_integer;
    dummy_int.size = 4;
    dummy_int.variant.integer.int_kind = ik_int;

    a_type dummy_ptr = {};
    dummy_ptr.kind = tk_pointer;
    dummy_ptr.size = 8;
    dummy_ptr.variant.pointer.type = &dummy_int;

    llvm::DIType* di_ty = nullptr;
    err = get_or_create_di_type(dbg_state, &dummy_ptr, &di_ty);
    assert(err == llvm_gen_be_error_t::ok);
    assert(di_ty != nullptr);
  }

  // Variable Tracking
  {
    a_type dummy_int = {};
    dummy_int.kind = tk_integer;
    dummy_int.size = 4;
    dummy_int.variant.integer.int_kind = ik_int;

    a_variable dummy_var = {};
    dummy_var.type = &dummy_int;
    dummy_var.source_corresp.name = "my_var";
    dummy_var.source_corresp.decl_position.seq = 0;
    dummy_var.is_parameter = 0;

    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
    llvm::Function* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", be_state->module.get());
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", fn);
    be_state->builder->SetInsertPoint(bb);
    
    llvm::AllocaInst* alloca_inst = be_state->builder->CreateAlloca(llvm::Type::getInt32Ty(*be_state->context));

    err = emit_dbg_declare_for_variable(dbg_state, &dummy_var, alloca_inst);
    assert(err == llvm_gen_be_error_t::ok);
  }

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