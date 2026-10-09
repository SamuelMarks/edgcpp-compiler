#include <cassert>
#include <cstdio>
#include <memory>
#include "llvm_gen_be_debug.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

BEGIN_EDG_NAMESPACE

void run_tests() {
  printf("Running debug_init_test...\n");

  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("test_module", context);

  // Invalid Argument
  {
    llvm_gen_be_debug_state_t* dbg_state = nullptr;
    llvm_gen_be_error_t err = debug_info_init(nullptr, module.get(), "test.cpp", ".", false);
    assert(err == llvm_gen_be_error_t::invalid_argument);

    err = debug_info_init(&dbg_state, nullptr, "test.cpp", ".", false);
    assert(err == llvm_gen_be_error_t::invalid_argument);

    err = debug_info_init(&dbg_state, module.get(), nullptr, ".", false);
    assert(err == llvm_gen_be_error_t::invalid_argument);
  }

  // InitializationAndFinalization
  {
    llvm_gen_be_debug_state_t* dbg_state = nullptr;
    llvm_gen_be_error_t err = debug_info_init(&dbg_state, module.get(), "test.cpp", ".", false);
    assert(err == llvm_gen_be_error_t::ok);
    assert(dbg_state != nullptr);
    assert(dbg_state->builder != nullptr);
    assert(dbg_state->compile_unit != nullptr);

    err = debug_info_finalize(dbg_state);
    assert(err == llvm_gen_be_error_t::ok);

    err = debug_info_cleanup(&dbg_state);
    assert(err == llvm_gen_be_error_t::ok);
    assert(dbg_state == nullptr);
  }

  // FinalizeInvalid
  {
    assert(debug_info_finalize(nullptr) == llvm_gen_be_error_t::invalid_argument);
  }

  // CleanupInvalid
  {
    assert(debug_info_cleanup(nullptr) == llvm_gen_be_error_t::invalid_argument);
    llvm_gen_be_debug_state_t* null_state = nullptr;
    assert(debug_info_cleanup(&null_state) == llvm_gen_be_error_t::invalid_argument);
  }

  // FileResolution
  {
    llvm_gen_be_debug_state_t* dbg_state = nullptr;
    llvm_gen_be_error_t err = debug_info_init(&dbg_state, module.get(), "test.cpp", ".", false);
    assert(err == llvm_gen_be_error_t::ok);

    llvm::DIFile* di_file1 = nullptr;
    err = get_or_create_di_file(dbg_state, "dir/source.cpp", &di_file1);
    assert(err == llvm_gen_be_error_t::ok);
    assert(di_file1 != nullptr);

    llvm::DIFile* di_file2 = nullptr;
    err = get_or_create_di_file(dbg_state, "dir/source.cpp", &di_file2);
    assert(err == llvm_gen_be_error_t::ok);
    assert(di_file1 == di_file2); // Should be cached

    assert(get_or_create_di_file(nullptr, "test.cpp", &di_file1) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_file(dbg_state, nullptr, &di_file1) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_file(dbg_state, "test.cpp", nullptr) == llvm_gen_be_error_t::invalid_argument);

    err = debug_info_cleanup(&dbg_state);
    assert(err == llvm_gen_be_error_t::ok);
  }

  printf("All tests passed!\n");
}

END_EDG_NAMESPACE

int main() {
  edg::run_tests();
  return 0;
}