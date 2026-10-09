#include <cassert>
#include <cstdio>
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_internal.h"
#include "types.h"
#include "il.h"

BEGIN_EDG_NAMESPACE

LLVMBackendState* be_state = nullptr;
a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
unsigned int targ_char_bit = 8;
a_targ_size_t targ_sizeof_short = 2;
a_targ_size_t targ_sizeof_int = 4;
a_targ_size_t targ_sizeof_long = 8;
a_targ_size_t targ_sizeof_long_long = 8;
a_targ_size_t targ_sizeof_pointer = 8;

a_boolean is_trivially_copyable_type(a_type_ptr ty) {
  // For tests, let's say size > 16 means non-trivial.
  return ty->size <= 16;
}

void run_tests() {
  printf("Running abi_win64_test...\n");

  // Invalid argument
  {
    win64_arg_info_t info;
    llvm_gen_be_error_t err = classify_win64_argument(nullptr, &info);
    assert(err == llvm_gen_be_error_t::invalid_argument);
  }

  // Integer Types
  {
    a_type ty = {};
    ty.size = 4;
    ty.kind = tk_integer;
    
    win64_arg_info_t info;
    llvm_gen_be_error_t err = classify_win64_argument(&ty, &info);
    
    assert(err == llvm_gen_be_error_t::ok);
    assert(info.abi_class == win64_abi_class_t::direct_integer);
    assert(!info.is_indirect);
    assert(info.size == 4);
  }

  // Float Types
  {
    a_type ty = {};
    ty.size = 8;
    ty.kind = tk_float;
    
    win64_arg_info_t info;
    llvm_gen_be_error_t err = classify_win64_argument(&ty, &info);
    
    assert(err == llvm_gen_be_error_t::ok);
    assert(info.abi_class == win64_abi_class_t::direct_float);
    assert(!info.is_indirect);
  }

  // Large Struct Classifies As Indirect
  {
    a_type ty = {};
    ty.size = 24;
    ty.kind = tk_class;
    
    win64_arg_info_t info;
    llvm_gen_be_error_t err = classify_win64_argument(&ty, &info);
    
    assert(err == llvm_gen_be_error_t::ok);
    assert(info.abi_class == win64_abi_class_t::indirect_by_pointer);
    assert(info.is_indirect);
  }

  // Eight-Byte Struct Classifies As Direct
  {
    a_type ty = {};
    ty.size = 8;
    ty.kind = tk_class;
    
    win64_arg_info_t info;
    llvm_gen_be_error_t err = classify_win64_argument(&ty, &info);
    
    assert(err == llvm_gen_be_error_t::ok);
    assert(info.abi_class == win64_abi_class_t::direct_integer);
    assert(!info.is_indirect);
  }

  // Array Classifies As Indirect
  {
    a_type ty = {};
    ty.size = 8;
    ty.kind = tk_array;
    
    win64_arg_info_t info;
    llvm_gen_be_error_t err = classify_win64_argument(&ty, &info);
    
    assert(err == llvm_gen_be_error_t::ok);
    assert(info.abi_class == win64_abi_class_t::indirect_by_pointer);
    assert(info.is_indirect);
  }
  
  printf("All tests passed!\n");
}

END_EDG_NAMESPACE

int main() {
  edg::run_tests();
  return 0;
}