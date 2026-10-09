#include "llvm_gen_be_opt.h"
#include <iostream>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace edg;

int main() {
  int fail_count = 0;

  // Test 1: parse_opt_level_string
  llvm_opt_options_t opts;
  if (parse_opt_level_string("O3", &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: Failed to parse O3\n";
    fail_count++;
  } else if (opts.opt_level != llvm_opt_level_t::O3 || !opts.vectorize_loops || !opts.vectorize_slp) {
    std::cerr << "FAIL: O3 options not correct\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string O3\n";
  }

  if (parse_opt_level_string("O0", &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: Failed to parse O0\n";
    fail_count++;
  } else if (opts.opt_level != llvm_opt_level_t::O0 || opts.vectorize_loops || opts.vectorize_slp) {
    std::cerr << "FAIL: O0 options not correct\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string O0\n";
  }
  
  if (parse_opt_level_string("O1", &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: Failed to parse O1\n";
    fail_count++;
  } else if (opts.opt_level != llvm_opt_level_t::O1 || opts.vectorize_loops || !opts.vectorize_slp) {
    std::cerr << "FAIL: O1 options not correct\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string O1\n";
  }
  
  if (parse_opt_level_string("O2", &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: Failed to parse O2\n";
    fail_count++;
  } else if (opts.opt_level != llvm_opt_level_t::O2 || !opts.vectorize_loops || !opts.vectorize_slp) {
    std::cerr << "FAIL: O2 options not correct\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string O2\n";
  }

  if (parse_opt_level_string("Os", &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: Failed to parse Os\n";
    fail_count++;
  } else if (opts.opt_level != llvm_opt_level_t::Os || opts.vectorize_loops || !opts.vectorize_slp) {
    std::cerr << "FAIL: Os options not correct\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string Os\n";
  }

  if (parse_opt_level_string("Oz", &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: Failed to parse Oz\n";
    fail_count++;
  } else if (opts.opt_level != llvm_opt_level_t::Oz || opts.vectorize_loops || opts.vectorize_slp) {
    std::cerr << "FAIL: Oz options not correct\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string Oz\n";
  }

  if (parse_opt_level_string("O4", &opts) != llvm_gen_be_error_t::invalid_argument) {
    std::cerr << "FAIL: parse_opt_level_string should fail on invalid level\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string invalid argument\n";
  }

  if (parse_opt_level_string(nullptr, &opts) != llvm_gen_be_error_t::invalid_argument) {
    std::cerr << "FAIL: parse_opt_level_string should fail on null str\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string null str\n";
  }
  
  if (parse_opt_level_string("O3", nullptr) != llvm_gen_be_error_t::invalid_argument) {
    std::cerr << "FAIL: parse_opt_level_string should fail on null out_opts\n";
    fail_count++;
  } else {
    std::cout << "PASS: parse_opt_level_string null out_opts\n";
  }

  // Test 2: run_optimization_pipeline
  llvm::LLVMContext context;
  llvm::Module module("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create a simple function that can be optimized (dead code elimination)
  llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt32Ty(), false);
  llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "test_func", &module);
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
  builder.SetInsertPoint(entry);
  llvm::Value* add1 = builder.CreateAdd(builder.getInt32(1), builder.getInt32(2));
  llvm::Value* add2 = builder.CreateAdd(builder.getInt32(3), builder.getInt32(4)); // Dead code
  builder.CreateRet(add1);

  if (llvm::verifyModule(module, &llvm::errs())) {
    std::cerr << "FAIL: Module verification failed\n";
    fail_count++;
  }

  if (run_optimization_pipeline(&module, &opts) != llvm_gen_be_error_t::ok) {
    std::cerr << "FAIL: run_optimization_pipeline failed\n";
    fail_count++;
  } else {
    std::cout << "PASS: run_optimization_pipeline\n";
  }
  
  if (run_optimization_pipeline(nullptr, &opts) != llvm_gen_be_error_t::invalid_argument) {
    std::cerr << "FAIL: run_optimization_pipeline null module\n";
    fail_count++;
  } else {
    std::cout << "PASS: run_optimization_pipeline null module\n";
  }
  
  if (run_optimization_pipeline(&module, nullptr) != llvm_gen_be_error_t::invalid_argument) {
    std::cerr << "FAIL: run_optimization_pipeline null opts\n";
    fail_count++;
  } else {
    std::cout << "PASS: run_optimization_pipeline null opts\n";
  }

  if (fail_count == 0) {
    std::cout << "All tests passed.\n";
    return 0;
  } else {
    std::cerr << fail_count << " tests failed.\n";
    return 1;
  }
}
