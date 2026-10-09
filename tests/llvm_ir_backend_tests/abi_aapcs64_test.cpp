#include <gtest/gtest.h>
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_internal.h"
#include "types.h"
#include "il.h"

BEGIN_EDG_NAMESPACE

class ABIAAPCS64Test : public ::testing::Test {
protected:
  void SetUp() override {
  }

  void TearDown() override {
  }
};

TEST_F(ABIAAPCS64Test, EmptyTypeHasZeroElements) {
  a_type ty = {};
  ty.size = 0;
  
  aapcs64_arg_info_t info;
  llvm_gen_be_error_t err = classify_aapcs64_argument(&ty, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_EQ(info.num_elements, 0);
}

TEST_F(ABIAAPCS64Test, IntegerClassification) {
  a_type ty = {};
  ty.size = 8;
  ty.kind = tk_integer;
  
  aapcs64_arg_info_t info;
  llvm_gen_be_error_t err = classify_aapcs64_argument(&ty, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_EQ(info.abi_class, aapcs64_abi_class_t::integer);
  EXPECT_EQ(info.num_elements, 1);
}

TEST_F(ABIAAPCS64Test, HFADetection) {
  a_type ty = {};
  ty.size = 16;
  ty.kind = tk_struct;
  
  a_type fty = {};
  fty.size = 4;
  fty.kind = tk_float;
  fty.variant.float_kind = fk_float;
  fty.alignment = 4;
  
  a_field field2 = {};
  field2.type = &fty;
  field2.offset = 4;
  field2.next = nullptr;
  field2.is_bit_field = false;

  a_field field1 = {};
  field1.type = &fty;
  field1.offset = 0;
  field1.next = &field2;
  field1.is_bit_field = false;

  ty.variant.class_struct_union.field_list = &field1;

  aapcs64_arg_info_t info;
  llvm_gen_be_error_t err = classify_aapcs64_argument(&ty, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_EQ(info.abi_class, aapcs64_abi_class_t::hfa_hva);
  EXPECT_EQ(info.num_elements, 2);
}

TEST_F(ABIAAPCS64Test, OversizedStructIsReference) {
  a_type ty = {};
  ty.size = 32;
  ty.kind = tk_struct;
  ty.variant.class_struct_union.field_list = nullptr;
  
  aapcs64_arg_info_t info;
  llvm_gen_be_error_t err = classify_aapcs64_argument(&ty, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_EQ(info.abi_class, aapcs64_abi_class_t::reference);
}

END_EDG_NAMESPACE
